/****************************************************************************
 *
 * MODULE:       r.ros
 * AUTHOR(S):    Jianping Xu, Rutgers University, 1993
 *               Markus Neteler <neteler itc.it>
 *               Roberto Flor <flor itc.it>,
 *               Brad Douglas <rez touchofmadness.com>,
 *               Glynn Clements <glynn gclements.plus.com>,
 *               Jachym Cepicky <jachym les-ejk.cz>
 * PURPOSE:      Generates rate of spread raster map layers
 *               for wildfire modeling
 *
 * TODO: (re)move following documentation
 *
 * This raster module creates three raster map layers:
 *        1. Base (perpendicular) rate of spread (ROS);
 *        2. Maximum (forward) ROS; and
 *        3. Direction of the Maximum ROS.
 * The calculation of the two ROS values for each raster
 * cell is based on the Fortran code by Pat Andrews (1983)
 * of the Northern Forest Fire Laboratory, USDA Forest
 * Service. These three raster map layers are expected to
 * be the inputs for a separate GRASS raster module
 * 'r.spread'.
 *
 * 'r.ros' can be run in two standard GRASS modes:
 * interactive and command line. For an interactive run,
 * type in
 *        r.ros
 * and follow the prompts; for a command line mode,
 * type in
 *         r.ros [-v] model=name [moisture_1h=name]
 *                        [moisture_10h=name]
 *                        [moisture_100h=name]
 *                        moisture_live=name [velocity=name]
 *                        [direction=name] [slope=name]
 *                        [aspect=name] output=name
 * where:
 *   Flag:
 *   Raster Maps:
 *      model                1-13: the standard fuel models,
 *              all other numbers: same as barriers;
 *        moisture_1h           100*moisture_content;
 *        moisture_10h          100*moisture_content;
 *        moisture_100h      100*moisture_content;
 *        moisture_live      100*moisture_content;
 *        velocity           ft/minute;
 *        direction          degree;
 *        slope              degree;
 *        aspect             degree starting from East, anti-clockwise;
 *        output
 *          for Base ROS        cm/min (technically not ft/min);
 *          for Max ROS         cm/min (technically not ft/min);
 *          for Direction       degree.
 *
 * Note that the name given as output will be used as
 * PREFIX for several actual raster maps. For example, if
 * my_ros is given as the output name, 'r.ros' will
 * actually produce three raster maps named my_ros.base,
 * my_ros.max, and my_ros.maxdir, or even my_ros.spotdist
 * respectively.
 *
 * COPYRIGHT:    (C) 2000-2009 by the GRASS Development Team
 *
 *               This program is free software under the GNU General Public
 *               License (>=v2). Read the file COPYING that comes with GRASS
 *               for details.
 *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <grass/gis.h>
#include <grass/raster.h>
#include <grass/glocale.h>
#include "local_proto.h"

#define DATA(map, r, c) (map)[(r)*ncols + (c)]

/*
//measurements of the 13 fuel models, input of Rothermel equation (1972)
float WO[4][14] = {{0, 0.034, 0.092, 0.138, 0.230, 0.046, 0.069, 0.052, 0.069,
                    0.134, 0.138, 0.069, 0.184, 0.322},
                   {0, 0, 0.046, 0, 0.184, 0.023, 0.115, 0.086, 0.046, 0.019,
                    0.092, 0.207, 0.644, 1.058},
                   {0, 0, 0.023, 0, 0.092, 0, 0.092, 0.069, 0.115, 0.007, 0.230,
                    0.253, 0.759, 1.288},
                   {0, 0, 0.023, 0, 0.230, 0.092, 0, 0.017, 0, 0, 0.092, 0, 0}};

//ovendry fuel loading, lb./ft.^2 
float DELTA[] = {0,   1.0, 1.0, 2.5, 6.0, 2.0, 2.5,
                 2.5, 0.2, 0.2, 1.0, 1.0, 2.3, 3.0}; /*fuel depth, ft. 

float SIGMA[4][14] = {
    {0, 3500, 3000, 1500, 2000, 2000, 1750, 1750, 2000, 2500, 2000, 1500, 1500,
     1500},
    {0, 0, 109, 0, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109},
    {0, 0, 30, 0, 30, 0, 30, 30, 30, 30, 30, 30, 30, 30},
    {0, 0, 1500, 0, 1500, 1500, 0, 1500, 0, 0, 1500, 0, 0, 0}};

//fuel particle surface-area-to-volume ratio, 1/ft. 
float MX[] = {
    0,    0.12, 0.15, 0.25, 0.20, 0.20, 0.25, 0.40,
    0.30, 0.25, 0.25, 0.15, 0.20, 0.25}; //moisture content of extinction 
*/

float WO[4][41] =
    { {0, 0.034, 0.092, 0.138, 0.230, 0.046, 0.069, 0.052, 0.069, 0.134,
       0.138, 0.069, 0.184, 0.322, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0.046, 0, 0.184, 0.023, 0.115, 0.086, 0.046, 0.019, 0.092, 0.207,
 0.644, 1.058, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0.023, 0, 0.092, 0, 0.092, 0.069, 0.115, 0.007, 0.230, 0.253, 0.759,
 1.288, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0.023, 0, 0.230, 0.092, 0, 0.017, 0, 0, 0.092, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};/*ovendry fuel loading, lb./ft.^2 */

float DELTA[] = { 0, 1.0, 1.0, 2.5, 6.0, 2.0, 2.5, 2.5,
    0.2, 0.2, 1.0, 1.0, 2.3, 3.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};/*fuel depth, ft. */

float SIGMA[4][41] =
    { {0, 3500, 3000, 1500, 2000, 2000, 1750, 1750, 2000, 2500, 2000, 1500,
       1500, 1500, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 109, 0, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 30, 0, 30, 0, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
	   30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
{0, 0, 1500, 0, 1500, 1500, 0, 1500, 0, 0, 1500, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
/*citation:
For all fuel models in this new set:
� 10-hr dead fuel SAV is 109 1/ft, and 100-hr SAV is 30 1/ft
*/
/*fuel particale surface-area-to-volume ratio, 1/ft. */
float MX[] = { 0, 0.12, 0.15, 0.25, 0.20, 0.20, 0.25, 0.40,
    0.30, 0.25, 0.25, 0.15, 0.20, 0.25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};/*moisture content of extinction */

CELL *map_elev; /*full array for elevation map layer (for spotting) */
int nrows, ncols;
struct Cell_head window;

/*** Dodao Marin */
//Number of fuel categories, by default 13 Albini Anderson categories
int  numCategories = 13;
/*Kraj - Marin */

int main(int argc, char *argv[])
{

    /***input of Rothermel equation (1972)***/
    float h = 8000.0, /*heat of combustion, BTU/lb. */
        rhop = 32,    /*ovendry fuel density, lb./ft.^3 */
        ST = 0.0555;  /*fuel total mineral content, lb. minerals/lb. ovendry */

    float sigma[40];

    /***derived parameters of Rothermel equation (1972)***/
    float R,                            /*rate of spread, ft./min.
                                           R = IR*xi*(1+phiw+phis)/(rhob*epsilon*Qig)  */
        IR,                             /*reaction intensity, BTU/ft.^2/min.
                                           IR = gamma*wn*h*etaM*etas  */
        gamma,                          /*optimum reation velosity, 1/min.
                                           gamma = gammamax*(beta/betaop)^A*
                                           exp(A(1-beta/betaop))  */
        gammamax,                       /*maximum reation velosity, 1/min.
                                           gammamax = sigma^1.5/(495+0.0594*sigma^1.5)  */
        betaop,                         /*optimum packing ratio
                                           betaop = 3.348/(sigma^0.8189)  */
        A,                              /*A = 1/(4.77*sigma^0.1-7.27)  */
        etas = 0.174 / pow(0.01, 0.19), /*mineral damping coefficient */
        xi,                             /*propagating flux ratio,
                                           xi = exp((0.792+0.681*sigma^0.5)(beta+0.1))/
                                           (192+0.2595*sigma)  */
        phiw, /*wind coefficient,  phiw = C*U^B/(beta/betaop)^E */
        C,    /*C = 7.47/exp(0.133*sigma^0.55)  */
        B,    /*B = 0.02526*sigma^.054   */
        E,    /*E = 0.715/exp(0.000359*sigma)  */
        phis, /*slope coefficient,phis = 5.275/beta^0.3*(tan(theta)^2) */
        rhob, /*ovendry bulk density, lb./ft.^3, rohb = wo/delta */
        epsilon[4]
               [41], /*effective heating number, epsilon = 1/exp(138/sigma) */
        Qig[41],     /*heat of preignition, BTU/lb.  Qig = 250+1116*Mf */
        beta;        /*packing ratio,  beta = rhob/rhop  */

    /***intermediate variables***/
    float R0, /*base ROS (w/out wind/slope) */
        Rdir, sin_fac, cos_fac,
        Ffactor_all[4][41],     /*in all fuel subclasses by sigma/WO */
        Ffactor_in_dead[3][41], /*in dead fuel subclasses by sigma/WO */
        /* Ffactor_dead[41], */ /*dead fuel weight by sigma/WO */
        /* Ffactor_live[41], */ /*live fuel weight by sigma/WO */
        Gfactor_in_dead[3][41], /*in dead fuel by the 6 classes */
        G1, G2, G3, G4, G5, wo_dead[41], /*dead fuel total load */
        wn_dead,                         /*net dead fuel total load */
        wn_live,                         /*net live fuel (total) load */
        class_sum, moisture[4], /*moistures of 1-h,10-h,100-h,live fuels */
        Mf_dead,                /*total moisture of dead fuels */
        etaM_dead,              /*dead fuel misture damping coefficient */
        etaM_live,              /*live fuel misture damping coefficient */
        xmext,                  /*live fuel moisture of extinction */
        phi_ws,                 /*wind and slope conbined coefficient */
        wmfd, fdmois, fined, finel;

    /*other local variables */
    int col, row, spotting, model, class,
        fuel_fd = 0, mois_1h_fd = 0, mois_10h_fd = 0, mois_100h_fd = 0,
        mois_live_fd = 0, vel_fd = 0, dir_fd = 0, elev_fd = 0, slope_fd = 0,
        aspect_fd = 0, base_fd = 0, max_fd = 0, maxdir_fd = 0, spotdist_fd = 0;

    char *name_base, *name_max, *name_maxdir, *name_spotdist;

    CELL *fuel,     /*cell buffer for fuel model map layer */
        *mois_1h,   /*cell buffer for 1-hour fuel moisture map layer */
        *mois_10h,  /*cell buffer for 10-hour fuel moisture map layer */
        *mois_100h, /*cell buffer for 100-hour fuel moisture map layer */
        *mois_live, /*cell buffer for live fuel moisture map layer */
        *vel,       /*cell buffer for wind velocity map layer */
        *dir,       /*cell buffer for wind direction map layer */
            *elev =
                NULL, /*cell buffer for elevation map layer (for spotting) */
        *slope,       /*cell buffer for slope map layer */
        *aspect,      /*cell buffer for aspect map layer */
        *base,        /*cell buffer for base ROS map layer */
        *max,         /*cell buffer for max ROS map layer */
        *maxdir,      /*cell buffer for max ROS direction map layer */
            *spotdist =
                NULL; /*cell buffer for max spotting distance map layer */

    extern struct Cell_head window;

    struct {
        struct Option *model, *mois_1h, *mois_10h, *mois_100h, *mois_live, *vel,
            *dir, *elev, *slope, *aspect, *base, *max, *maxdir, *spotdist, *external_param_file;
    } parm;

    /* please, remove before GRASS 7 released */
    struct GModule *module;

    /* initialize access to database and create temporary files */
    G_gisinit(argv[0]);

    /* Set description */
    module = G_define_module();
    G_add_keyword(_("raster"));
    G_add_keyword(_("fire"));
    G_add_keyword(_("spread"));
    G_add_keyword(_("rate of spread"));
    G_add_keyword(_("hazard"));
    G_add_keyword(_("model"));
    module->label = _("Generates rate of spread raster maps.");
    module->description =
        _("Generates three, or four raster map layers showing the base "
          "(perpendicular) rate of spread (ROS), the maximum (forward) ROS, "
          "the direction of the maximum ROS, and optionally the "
          "maximum potential spotting distance for fire spread simulation.");

    parm.model = G_define_standard_option(G_OPT_R_INPUT);
    parm.model->key = "model";
    parm.model->label = _("Raster map containing fuel models");
    parm.model->description =
        _("Name of an "
          "existing raster map layer in the user's current mapset search path "
          "containing "
          "the standard fuel models defined by the USDA Forest Service. Valid "
          "values "
          "are 1-13; other numbers are recognized as barriers by r.ros.");

    parm.mois_1h = G_define_standard_option(G_OPT_R_INPUT);
    parm.mois_1h->key = "moisture_1h";
    parm.mois_1h->required = NO;
    parm.mois_1h->label =
        _("Raster map containing the 1-hour fuel moisture (%)");
    parm.mois_1h->description = _(
        "Name of an existing raster map layer in "
        "the user's current mapset search path containing the 1-hour (<.25\") "
        "fuel moisture (percentage content multiplied by 100).");

    parm.mois_10h = G_define_standard_option(G_OPT_R_INPUT);
    parm.mois_10h->key = "moisture_10h";
    parm.mois_10h->required = NO;
    parm.mois_10h->label =
        _("Raster map containing the 10-hour fuel moisture (%)");
    parm.mois_10h->description =
        _("Name of an existing raster map layer in the "
          "user's current mapset search path containing the 10-hour (.25-1\") "
          "fuel "
          "moisture (percentage content multiplied by 100).");

    parm.mois_100h = G_define_standard_option(G_OPT_R_INPUT);
    parm.mois_100h->key = "moisture_100h";
    parm.mois_100h->required = NO;
    parm.mois_100h->label =
        _("Raster map containing the 100-hour fuel moisture (%)");
    parm.mois_100h->description =
        _("Name of an existing raster map layer in the "
          "user's current mapset search path containing the 100-hour (1-3\") "
          "fuel moisture "
          "(percentage content multiplied by 100).");

    parm.mois_live = G_define_standard_option(G_OPT_R_INPUT);
    parm.mois_live->key = "moisture_live";
    parm.mois_live->label = _("Raster map containing live fuel moisture (%)");
    parm.mois_live->description =
        _("Name of an existing raster map layer in the "
          "user's current mapset search path containing live (herbaceous) fuel "
          "moisture (percentage content multiplied by 100).");

    parm.vel = G_define_standard_option(G_OPT_R_INPUT);
    parm.vel->key = "velocity";
    parm.vel->required = NO;
    parm.vel->label =
        _("Raster map containing midflame wind velocities (ft/min)");
    parm.vel->description =
        _("Name of an existing raster map layer in the user's "
          "current mapset search path containing wind velocities at half of "
          "the average "
          "flame height (feet/minute).");

    parm.dir = G_define_standard_option(G_OPT_R_INPUT);
    parm.dir->key = "direction";
    parm.dir->required = NO;
    parm.dir->label =
        _("Name of raster map containing wind directions (degree)");
    parm.dir->description = _("Name of an existing raster map "
                              "layer in the user's current mapset search path "
                              "containing wind direction, "
                              "clockwise from north (degree).");

    parm.slope = G_define_standard_option(G_OPT_R_INPUT);
    parm.slope->key = "slope";
    parm.slope->required = NO;
    parm.slope->label = _("Name of raster map containing slope (degree)");
    parm.slope->description =
        _("Name of an existing raster map layer "
          "in the user's current mapset search path containing "
          "topographic slope (degree).");

    parm.aspect = G_define_standard_option(G_OPT_R_INPUT);
    parm.aspect->key = "aspect";
    parm.aspect->required = NO;
    parm.aspect->label = _("Raster map containing aspect (degree, CCW from E)");
    parm.aspect->description = _(
        "Name of an existing "
        "raster map layer in the user's current mapset search path containing "
        "topographic aspect, counterclockwise from east (GRASS convention) "
        "in degrees.");

    parm.elev = G_define_standard_option(G_OPT_R_ELEV);
    parm.elev->required = NO;
    parm.elev->label =
        _("Raster map containing elevation (m, required for spotting)");
    parm.elev->description =
        _("Name of an existing raster map "
          "layer in the user's current mapset search path containing elevation "
          "(meters). "
          "Option is required from spotting distance computation "
          "(when spotting_distance option is provided)");

    parm.base = G_define_standard_option(G_OPT_R_OUTPUT);
    parm.base->key = "base_ros";
    parm.base->required = YES;
    parm.base->label = _("Output raster map containing base ROS (cm/min)");
    parm.base->description = _("Base (perpendicular) rate of spread (ROS)");

    parm.max = G_define_standard_option(G_OPT_R_OUTPUT);
    parm.max->key = "max_ros";
    parm.max->required = YES;
    parm.max->label = _("Output raster map containing maximal ROS (cm/min)");
    parm.max->description = _("The maximum (forward) rate of spread (ROS)");

    parm.maxdir = G_define_standard_option(G_OPT_R_OUTPUT);
    parm.maxdir->key = "direction_ros";
    parm.maxdir->required = YES;
    parm.maxdir->label =
        _("Output raster map containing directions of maximal ROS (degree)");
    parm.maxdir->description =
        _("The direction of the maximal (forward) rate of spread (ROS)");

    parm.spotdist = G_define_standard_option(G_OPT_R_OUTPUT);
    parm.spotdist->key = "spotting_distance";
    parm.spotdist->required = NO;
    parm.spotdist->label =
        _("Output raster map containing maximal spotting distance (m)");
    parm.spotdist->description =
        _("The maximal potential spotting distance"
          " (requires elevation raster map to be provided).");

	/*DODAO MARIN - POCETAK*/
    parm.external_param_file = G_define_option();
    parm.external_param_file->key = "external_param_file";
    parm.external_param_file->type = TYPE_STRING;
    //parm.external_param_file->gisprompt = "new,cell,raster";
    parm.external_param_file->description =
	_("Name of the external file with defined parameters for ROS ?!!!");	
	/*DODAO MARIN - KRAJ*/
	
    /*   Parse command line */
    if (G_parser(argc, argv))
        exit(EXIT_FAILURE);
	
	/* MARIN DODAO - POCETAK */
	if(parm.external_param_file->answer != NULL)
	{
		if( access( parm.external_param_file->answer, F_OK ) != -1 ) {
			
			G_warning(_("External parameter file <%s> chosen"), parm.external_param_file->answer);
			
					
			char buffer[256];
			FILE *fp;
			//float WO_new[4][14];
			int i=0;
			numCategories=0;
			
			if( (fp = fopen(parm.external_param_file->answer, "r+")) == NULL)
			{
				G_warning(_("No such file\n"));
				//exit(1);
			} 
			 if (fp == NULL)
			{
				G_warning(_("Error Reading File\n"));
			}		

			//Prvo odrediti o kojim je kategorijama rijec 13 ili 40
			FILE  *fp_pre = fopen(parm.external_param_file->answer, "r+"); 
			int                 c;              /* Nb. int (not char) for the EOF */

			
			/* count the newline characters */
			while ( (c=fgetc(fp_pre)) != '\n' ) {
				if ( c == ',' )
					numCategories++;
			}
			fclose(fp_pre);
			
					
			if (!(numCategories == 13 || numCategories == 40))
			{
				G_warning(_("Argument \"external_param_file\" - %s does not have neither 13 nor 40 categories! Choosing default parameters"), parm.external_param_file->answer);
			}			
			else
			{
				//Ako je rijec o Albini-Andersonu
				if(numCategories == 13)
				{				
					G_message(_("***** Modifying Albini-Anderson categories *****\n"));			

					
					//Prvo rijesiti W0 do W3 jer je to zapisano u prva tri reda
					for (i = 0; i < 3; i++){
						fgets(buffer, 512, fp);
						//printf("%d. \n", i);
						sscanf(buffer, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f", &WO[i][0], &WO[i][1], &WO[i][2], &WO[i][3], &WO[i][4], &WO[i][5], &WO[i][6], &WO[i][7], &WO[i][8], &WO[i][9], &WO[i][10], &WO[i][11], &WO[i][12], &WO[i][13] );
						//G_message(_("Uneseno je:\n"));
						//G_message(_("%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f"), WO[i][0], WO[i][1], WO[i][2], WO[i][3], WO[i][4], WO[i][5], WO[i][6], WO[i][7], WO[i][8], WO[i][9], WO[i][10], WO[i][11], WO[i][12], WO[i][13] );
						//G_message(_("\n"));
					}
					
					float tempW3[2][14] ={ {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} };

					
					//Sad pročitati sljedeća dva reda, zapamtiti rezultate i odabrati ve�eg 
					for (i = 0; i < 2; i++){
						fgets(buffer, 512, fp);
						sscanf(buffer, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f", &tempW3[i][0], &tempW3[i][1], &tempW3[i][2], &tempW3[i][3], &tempW3[i][4], &tempW3[i][5], &tempW3[i][6], &tempW3[i][7], &tempW3[i][8], &tempW3[i][9], &tempW3[i][10], &tempW3[i][11], &tempW3[i][12], &tempW3[i][13] );
					}
					//sad odabrati većeg
					for (i = 0; i < 14; i++){
						if(tempW3[0][i]>=tempW3[1][i])
							WO[3][i]=tempW3[0][i];
						else
							WO[3][i]=tempW3[1][i];
						
						//G_message(_("%f "), WO[3][i]);
					}
					
					for (i = 0; i < 1; i++){
						fgets(buffer, 512, fp);
						sscanf(buffer, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f", &DELTA[0], &DELTA[1], &DELTA[2], &DELTA[3], &DELTA[4], &DELTA[5], &DELTA[6], &DELTA[7], &DELTA[8], &DELTA[9], &DELTA[10], &DELTA[11], &DELTA[12], &DELTA[13] );
						//G_message(_("%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f"), DELTA[0], DELTA[1], DELTA[2], DELTA[3], DELTA[4], DELTA[5], DELTA[6], DELTA[7], DELTA[8], DELTA[9], DELTA[10], DELTA[11], DELTA[12], DELTA[13] );
						//G_message(_("\n"));
					}
					
					for (i = 0; i < 1; i++){
						fgets(buffer, 512, fp);
						//printf("%d. \n", i);
						sscanf(buffer, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f", &SIGMA[i][0], &SIGMA[i][1], &SIGMA[i][2], &SIGMA[i][3], &SIGMA[i][4], &SIGMA[i][5], &SIGMA[i][6], &SIGMA[i][7], &SIGMA[i][8], &SIGMA[i][9], &SIGMA[i][10], &SIGMA[i][11], &SIGMA[i][12], &SIGMA[i][13] );
						//G_message(_("%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f"), SIGMA[i][0], SIGMA[i][1], SIGMA[i][2], SIGMA[i][3], SIGMA[i][4], SIGMA[i][5], SIGMA[i][6], SIGMA[i][7], SIGMA[i][8], SIGMA[i][9], SIGMA[i][10], SIGMA[i][11], SIGMA[i][12], SIGMA[i][13] );
						//G_message(_("\n"));
					}
					
					//Za sada se zanemaruju sljede�a dva reda 
					for (i = 0; i < 2; i++){
						fgets(buffer, 512, fp);
						//printf("%d. \n", i);
						//sscanf(buffer, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f", &SIGMA[i][0], &SIGMA[i][1], &SIGMA[i][2], &SIGMA[i][3], &SIGMA[i][4], &SIGMA[i][5], &SIGMA[i][6], &SIGMA[i][7], &SIGMA[i][8], &SIGMA[i][9], &SIGMA[i][10], &SIGMA[i][11], &SIGMA[i][12], &SIGMA[i][13] );
						//G_message(_("%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f"), SIGMA[i][0], SIGMA[i][1], SIGMA[i][2], SIGMA[i][3], SIGMA[i][4], SIGMA[i][5], SIGMA[i][6], SIGMA[i][7], SIGMA[i][8], SIGMA[i][9], SIGMA[i][10], SIGMA[i][11], SIGMA[i][12], SIGMA[i][13] );
						//G_message(_("\n"));
					}
					
					
					for (i = 0; i < 1; i++){
						fgets(buffer, 512, fp);
						//printf("%d. \n", i);
						sscanf(buffer, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f", &MX[0], &MX[1], &MX[2], &MX[3], &MX[4], &MX[5], &MX[6], &MX[7], &MX[8], &MX[9], &MX[10], &MX[11], &MX[12], &MX[13] );
						//G_message(_("%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f"), MX[0], MX[1], MX[2], MX[3], MX[4], MX[5], MX[6], MX[7], MX[8], MX[9], MX[10], MX[11], MX[12], MX[13] );
						//G_message(_("\n"));
					}
					
					//Ostatak parametara se zanemaruje za sada ...
					
					G_message(_("***** Measurements of the 13 fuel models MODIFIED *****"));
				}				
				else if(numCategories == 40)
				{	
					G_message(_("***** Modifying Scott-Burgan categories *****\n"));	
					
					//Prvo rijesiti W0 do W3 jer je to zapisano u prva tri reda
					for (i = 0; i < 3; i++){
						fgets(buffer, 512, fp);
						//printf("%d. \n", i);
						sscanf(buffer, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f", &WO[i][0], &WO[i][1], &WO[i][2], &WO[i][3], &WO[i][4], &WO[i][5], &WO[i][6], &WO[i][7], &WO[i][8], &WO[i][9], &WO[i][10], &WO[i][11], &WO[i][12], &WO[i][13], &WO[i][14], &WO[i][15], &WO[i][16], &WO[i][17], &WO[i][18], &WO[i][19], &WO[i][20], &WO[i][21], &WO[i][22], &WO[i][23], &WO[i][24], &WO[i][25], &WO[i][26], &WO[i][27], &WO[i][28], &WO[i][29], &WO[i][30], &WO[i][31], &WO[i][32], &WO[i][33], &WO[i][34], &WO[i][35], &WO[i][36], &WO[i][37], &WO[i][38], &WO[i][39], &WO[i][40]);
						//G_message(_("%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f"), WO[i][0], WO[i][1], WO[i][2], WO[i][3], WO[i][4], WO[i][5], WO[i][6], WO[i][7], WO[i][8], WO[i][9], WO[i][10], WO[i][11], WO[i][12], WO[i][13], WO[i][14], WO[i][15], WO[i][16], WO[i][17], WO[i][18], WO[i][19], WO[i][20], WO[i][21], WO[i][22], WO[i][23], WO[i][24], WO[i][25], WO[i][26], WO[i][27], WO[i][28], WO[i][29], WO[i][30], WO[i][31], WO[i][32], WO[i][33], WO[i][34], WO[i][35], WO[i][36], WO[i][37], WO[i][38], WO[i][39], WO[i][40] );
						//G_message(_("\n"));
					}
					
					float tempW3[2][41] ={ {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} };

					
					//Sad pro�itati sljede�a dva reda, zapamtiti rezultate i odabrati ve�eg 
					for (i = 0; i < 2; i++){
						fgets(buffer, 512, fp);
						sscanf(buffer, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f", &tempW3[i][0], &tempW3[i][1], &tempW3[i][2], &tempW3[i][3], &tempW3[i][4], &tempW3[i][5], &tempW3[i][6], &tempW3[i][7], &tempW3[i][8], &tempW3[i][9], &tempW3[i][10], &tempW3[i][11], &tempW3[i][12], &tempW3[i][13], &tempW3[i][14], &tempW3[i][15], &tempW3[i][16], &tempW3[i][17], &tempW3[i][18], &tempW3[i][19], &tempW3[i][20], &tempW3[i][21], &tempW3[i][22], &tempW3[i][23], &tempW3[i][24], &tempW3[i][25], &tempW3[i][26], &tempW3[i][27], &tempW3[i][28], &tempW3[i][29], &tempW3[i][30], &tempW3[i][31], &tempW3[i][32], &tempW3[i][33], &tempW3[i][34], &tempW3[i][35], &tempW3[i][36], &tempW3[i][37], &tempW3[i][38], &tempW3[i][39], &tempW3[i][40]);
					}
					//sad odabrati ve�eg
					for (i = 0; i < 41; i++){
						if(tempW3[0][i]>=tempW3[1][i])
							WO[3][i]=tempW3[0][i];
						else
							WO[3][i]=tempW3[1][i];
						
						//G_message(_("%f "), WO[3][i]);
					}
					
					
					for (i = 0; i < 1; i++){
						fgets(buffer, 512, fp);
						sscanf(buffer, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f", &DELTA[0], &DELTA[1], &DELTA[2], &DELTA[3], &DELTA[4], &DELTA[5], &DELTA[6], &DELTA[7], &DELTA[8], &DELTA[9], &DELTA[10], &DELTA[11], &DELTA[12], &DELTA[13], &DELTA[14], &DELTA[15], &DELTA[16], &DELTA[17], &DELTA[18], &DELTA[19], &DELTA[20], &DELTA[21], &DELTA[22], &DELTA[23], &DELTA[24], &DELTA[25], &DELTA[26], &DELTA[27], &DELTA[28], &DELTA[29], &DELTA[30], &DELTA[31], &DELTA[32], &DELTA[33], &DELTA[34], &DELTA[35], &DELTA[36], &DELTA[37], &DELTA[38], &DELTA[39], &DELTA[40]);
						//G_message(_("%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f"), DELTA[0], DELTA[1], DELTA[2], DELTA[3], DELTA[4], DELTA[5], DELTA[6], DELTA[7], DELTA[8], DELTA[9], DELTA[10], DELTA[11], DELTA[12], DELTA[13], DELTA[14], DELTA[15], DELTA[16], DELTA[17], DELTA[18], DELTA[19], DELTA[20], DELTA[21], DELTA[22], DELTA[23], DELTA[24], DELTA[25], DELTA[26], DELTA[27], DELTA[28], DELTA[29], DELTA[30], DELTA[31], DELTA[32], DELTA[33], DELTA[34], DELTA[35], DELTA[36], DELTA[37], DELTA[38], DELTA[39], DELTA[40]);
						//G_message(_("\n"));
					}
					
					
					for (i = 0; i < 1; i++){
						fgets(buffer, 512, fp);
						//printf("%d. \n", i);
						sscanf(buffer, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f", &SIGMA[i][0], &SIGMA[i][1], &SIGMA[i][2], &SIGMA[i][3], &SIGMA[i][4], &SIGMA[i][5], &SIGMA[i][6], &SIGMA[i][7], &SIGMA[i][8], &SIGMA[i][9], &SIGMA[i][10], &SIGMA[i][11], &SIGMA[i][12], &SIGMA[i][13], &SIGMA[i][14], &SIGMA[i][15], &SIGMA[i][16], &SIGMA[i][17], &SIGMA[i][18], &SIGMA[i][19], &SIGMA[i][20], &SIGMA[i][21], &SIGMA[i][22], &SIGMA[i][23], &SIGMA[i][24], &SIGMA[i][25], &SIGMA[i][26], &SIGMA[i][27], &SIGMA[i][28], &SIGMA[i][29], &SIGMA[i][30], &SIGMA[i][31], &SIGMA[i][32], &SIGMA[i][33], &SIGMA[i][34], &SIGMA[i][35], &SIGMA[i][36], &SIGMA[i][37], &SIGMA[i][38], &SIGMA[i][39], &SIGMA[i][40]);
						//G_message(_("%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f"), SIGMA[i][0], SIGMA[i][1], SIGMA[i][2], SIGMA[i][3], SIGMA[i][4], SIGMA[i][5], SIGMA[i][6], SIGMA[i][7], SIGMA[i][8], SIGMA[i][9], SIGMA[i][10], SIGMA[i][11], SIGMA[i][12], SIGMA[i][13], SIGMA[i][14], SIGMA[i][15], SIGMA[i][16], SIGMA[i][17], SIGMA[i][18], SIGMA[i][19], SIGMA[i][20], SIGMA[i][21], SIGMA[i][22], SIGMA[i][23], SIGMA[i][24], SIGMA[i][25], SIGMA[i][26], SIGMA[i][27], SIGMA[i][28], SIGMA[i][29], SIGMA[i][30], SIGMA[i][31], SIGMA[i][32], SIGMA[i][33], SIGMA[i][34], SIGMA[i][35], SIGMA[i][36], SIGMA[i][37], SIGMA[i][38], SIGMA[i][39], SIGMA[i][40]);
						//G_message(_("\n"));
					}
					
					/*citation:
						For all fuel models in this new set:
						� 10-hr dead fuel SAV is 109 1/ft, and 100-hr SAV is 30 1/ft
						*/
					
					//Za sada se zanemaruju sljede�a dva reda 
					for (i = 0; i < 41; i++){
						SIGMA[2][i]=109;
						SIGMA[3][i]=30;
					}
					//G_message(_("%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f"), SIGMA[2][0], SIGMA[2][1], SIGMA[2][2], SIGMA[2][3], SIGMA[2][4], SIGMA[2][5], SIGMA[2][6], SIGMA[2][7], SIGMA[2][8], SIGMA[2][9], SIGMA[2][10], SIGMA[2][11], SIGMA[2][12], SIGMA[2][13], SIGMA[2][14], SIGMA[2][15], SIGMA[2][16], SIGMA[2][17], SIGMA[2][18], SIGMA[2][19], SIGMA[2][20], SIGMA[2][21], SIGMA[2][22], SIGMA[2][23], SIGMA[2][24], SIGMA[2][25], SIGMA[2][26], SIGMA[2][27], SIGMA[2][28], SIGMA[2][29], SIGMA[2][30], SIGMA[2][31], SIGMA[2][32], SIGMA[2][33], SIGMA[2][34], SIGMA[2][35], SIGMA[2][36], SIGMA[2][37], SIGMA[2][38], SIGMA[2][39], SIGMA[2][40]);
					//G_message(_("\n\n"));
					//G_message(_("%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f"), SIGMA[3][0], SIGMA[3][1], SIGMA[3][3], SIGMA[3][3], SIGMA[3][4], SIGMA[3][5], SIGMA[3][6], SIGMA[3][7], SIGMA[3][8], SIGMA[3][9], SIGMA[3][10], SIGMA[3][11], SIGMA[3][12], SIGMA[3][13], SIGMA[3][14], SIGMA[3][15], SIGMA[3][16], SIGMA[3][17], SIGMA[3][18], SIGMA[3][19], SIGMA[3][20], SIGMA[3][21], SIGMA[3][22], SIGMA[3][23], SIGMA[3][24], SIGMA[3][25], SIGMA[3][26], SIGMA[3][27], SIGMA[3][28], SIGMA[3][29], SIGMA[3][30], SIGMA[3][31], SIGMA[3][32], SIGMA[3][33], SIGMA[3][34], SIGMA[3][35], SIGMA[3][36], SIGMA[3][37], SIGMA[3][38], SIGMA[3][39], SIGMA[3][40]);
					
					
					for (i = 0; i < 1; i++){
						fgets(buffer, 512, fp);
						//printf("%d. \n", i);
						sscanf(buffer, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f", &MX[0], &MX[1], &MX[2], &MX[3], &MX[4], &MX[5], &MX[6], &MX[7], &MX[8], &MX[9], &MX[10], &MX[11], &MX[12], &MX[13], &MX[14], &MX[15], &MX[16], &MX[17], &MX[18], &MX[19], &MX[20], &MX[21], &MX[22], &MX[23], &MX[24], &MX[25], &MX[26], &MX[27], &MX[28], &MX[29], &MX[30], &MX[31], &MX[32], &MX[33], &MX[34], &MX[35], &MX[36], &MX[37], &MX[38], &MX[39], &MX[40]);
						//G_message(_("%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f"), MX[0], MX[1], MX[2], MX[3], MX[4], MX[5], MX[6], MX[7], MX[8], MX[9], MX[10], MX[11], MX[12], MX[13], MX[14], MX[15], MX[16], MX[17], MX[18], MX[19], MX[20], MX[21], MX[22], MX[23], MX[24], MX[25], MX[26], MX[27], MX[28], MX[29], MX[30], MX[31], MX[32], MX[33], MX[34], MX[35], MX[36], MX[37], MX[38], MX[39], MX[40]);
						//G_message(_("\n"));
					}
					
					//Ostatak parametara se zanemaruje za sada ...
					
					G_message(_("***** Measurements of the 40 fuel models MODIFIED *****"));
			
				}
			}
				
		} else {
			// file doesn't exist
			G_warning(_("Argument \"external_param_file\" - %s doesn't exist! Choosing default parameters"), parm.external_param_file->answer);
		}

	}
	/* MARIN DODAO - KRAJ */
	
    if (parm.spotdist->answer)
        spotting = 1;
    else
        spotting = 0;

    /*  Check if input layers exists in data base  */
    if (G_find_raster2(parm.model->answer, "") == NULL)
        G_fatal_error(_("Raster map <%s> not found"), parm.model->answer);

    if (!(parm.mois_1h->answer || parm.mois_10h->answer ||
          parm.mois_100h->answer)) {
        G_fatal_error(_("No dead fuel moisture is given. "
                        "At least one of the 1-h, 10-h, 100-h moisture layers "
                        "is required."));
    }

    if (parm.mois_1h->answer) {
        if (G_find_raster2(parm.mois_1h->answer, "") == NULL)
            G_fatal_error(_("Raster map <%s> not found"), parm.mois_1h->answer);
    }
    if (parm.mois_10h->answer) {
        if (G_find_raster2(parm.mois_10h->answer, "") == NULL)
            G_fatal_error(_("Raster map <%s> not found"),
                          parm.mois_10h->answer);
    }
    if (parm.mois_100h->answer) {
        if (G_find_raster2(parm.mois_100h->answer, "") == NULL)
            G_fatal_error(_("Raster map <%s> not found"),
                          parm.mois_100h->answer);
    }

    if (G_find_raster2(parm.mois_live->answer, "") == NULL)
        G_fatal_error(_("Raster map <%s> not found"), parm.mois_live->answer);

    if (parm.vel->answer && !(parm.dir->answer)) {
        G_fatal_error(_("A wind direction layer should be "
                        "given if the wind velocity layer <%s> has been given"),
                      parm.vel->answer);
    }
    if (!(parm.vel->answer) && parm.dir->answer) {
        G_fatal_error(_("A wind velocity layer should be given "
                        "if the wind direction layer <%s> has been given"),
                      parm.dir->answer);
    }
    if (parm.vel->answer) {
        if (G_find_raster2(parm.vel->answer, "") == NULL)
            G_fatal_error(_("Raster map <%s> not found"), parm.vel->answer);
    }
    if (parm.dir->answer) {
        if (G_find_raster2(parm.dir->answer, "") == NULL)
            G_fatal_error(_("Raster map <%s> not found"), parm.dir->answer);
    }

    if (parm.slope->answer && !(parm.aspect->answer)) {
        G_fatal_error(_("An aspect layer should be given "
                        "if the slope layer <%s> has been given"),
                      parm.slope->answer);
    }
    if (!(parm.slope->answer) && parm.aspect->answer) {
        G_fatal_error(_("A slope layer should be given "
                        "if the aspect layer <%s> has been given"),
                      parm.aspect->answer);
    }
    if (parm.slope->answer) {
        if (G_find_raster2(parm.slope->answer, "") == NULL)
            G_fatal_error(_("Raster map <%s> not found"), parm.slope->answer);
    }
    if (parm.aspect->answer) {
        if (G_find_raster2(parm.aspect->answer, "") == NULL)
            G_fatal_error(_("Raster map <%s> not found"), parm.aspect->answer);
    }

    if (spotting) {
        if (!(parm.elev->answer)) {
            G_fatal_error(_("An elevation layer should be given "
                            "if considering spotting"));
        }
        else {
            if (G_find_raster2(parm.elev->answer, "") == NULL)
                G_fatal_error(_("Raster map <%s> not found"),
                              parm.elev->answer);
        }
    }

    /*assign names of the three output ROS layers */
    name_base = parm.base->answer;
    name_max = parm.max->answer;
    name_maxdir = parm.maxdir->answer;

    /*assign a name to output SPOTTING distance layer */
    if (spotting) {
        name_spotdist = parm.spotdist->answer;
    }

    /*  Get database window parameters  */
    G_get_window(&window);

    /*  find number of rows and columns in window    */
    nrows = Rast_window_rows();
    ncols = Rast_window_cols();

    fuel = Rast_allocate_c_buf();
    mois_1h = Rast_allocate_c_buf();
    mois_10h = Rast_allocate_c_buf();
    mois_100h = Rast_allocate_c_buf();
    mois_live = Rast_allocate_c_buf();
    vel = Rast_allocate_c_buf();
    dir = Rast_allocate_c_buf();
    slope = Rast_allocate_c_buf();
    aspect = Rast_allocate_c_buf();
    base = Rast_allocate_c_buf();
    max = Rast_allocate_c_buf();
    maxdir = Rast_allocate_c_buf();
    if (spotting) {
        spotdist = Rast_allocate_c_buf();
        elev = Rast_allocate_c_buf();
        map_elev = (CELL *)G_calloc(nrows * ncols, sizeof(CELL));
    }

    /*  Open input cell layers for reading  */

    fuel_fd = Rast_open_old(parm.model->answer, "");

    if (parm.mois_1h->answer)
        mois_1h_fd = Rast_open_old(parm.mois_1h->answer, "");

    if (parm.mois_10h->answer)
        mois_10h_fd = Rast_open_old(parm.mois_10h->answer, "");

    if (parm.mois_100h->answer)
        mois_100h_fd = Rast_open_old(parm.mois_100h->answer, "");

    mois_live_fd = Rast_open_old(parm.mois_live->answer, "");

    if (parm.vel->answer)
        vel_fd = Rast_open_old(parm.vel->answer, "");

    if (parm.dir->answer)
        dir_fd = Rast_open_old(parm.dir->answer, "");

    if (parm.slope->answer)
        slope_fd = Rast_open_old(parm.slope->answer, "");

    if (parm.aspect->answer)
        aspect_fd = Rast_open_old(parm.aspect->answer, "");

    if (spotting)
        elev_fd = Rast_open_old(parm.elev->answer, "");

    base_fd = Rast_open_c_new(name_base);
    max_fd = Rast_open_c_new(name_max);
    maxdir_fd = Rast_open_c_new(name_maxdir);
    if (spotting)
        spotdist_fd = Rast_open_c_new(name_spotdist);

    /*compute weights, combined wo, and combined sigma */
    /*wo[model] -- simple sum of WO[class][model] by all fuel subCLASS */
    /*sigma[model] -- weighted sum of SIGMA[class][model] by all fuel subCLASS
     * *epsilon[class][model] */
    for (model = 1; model <= numCategories; model++) {
        class_sum = 0.0;
        wo_dead[model] = 0.0;
        sigma[model] = 0.0;
        for (class = 0; class <= 3; class ++) {
            class_sum = class_sum + WO[class][model] * SIGMA[class][model];
            if (SIGMA[class][model] > 0.0) {
                epsilon[class][model] = exp(-138.0 / SIGMA[class][model]);
            }
            else {
                epsilon[class][model] = 0.0;
            }
        }
        for (class = 0; class <= 3; class ++) {
            Ffactor_all[class][model] =
                WO[class][model] * SIGMA[class][model] / class_sum;
            sigma[model] =
                sigma[model] + SIGMA[class][model] * Ffactor_all[class][model];
        }
        class_sum = 0.0;
        for (class = 0; class <= 2; class ++) {
            wo_dead[model] = wo_dead[model] + WO[class][model];
            class_sum = class_sum + WO[class][model] * SIGMA[class][model];
        }
        for (class = 0; class <= 2; class ++) {
            Ffactor_in_dead[class][model] =
                WO[class][model] * SIGMA[class][model] / class_sum;
        }

        /* compute G factor for each of the 6 subclasses */
        G1 = 0.0;
        G2 = 0.0;
        G3 = 0.0;
        G4 = 0.0;
        G5 = 0.0;
        for (class = 0; class <= 2; class ++) {
            if (SIGMA[class][model] >= 1200)
                G1 = G1 + Ffactor_in_dead[class][model];
            if (SIGMA[class][model] < 1200 && SIGMA[class][model] >= 192)
                G2 = G2 + Ffactor_in_dead[class][model];
            if (SIGMA[class][model] < 192 && SIGMA[class][model] >= 96)
                G3 = G3 + Ffactor_in_dead[class][model];
            if (SIGMA[class][model] < 96 && SIGMA[class][model] >= 48)
                G4 = G4 + Ffactor_in_dead[class][model];
            if (SIGMA[class][model] < 48 && SIGMA[class][model] >= 16)
                G5 = G5 + Ffactor_in_dead[class][model];
        }
        for (class = 0; class <= 2; class ++) {
            if (SIGMA[class][model] >= 1200)
                Gfactor_in_dead[class][model] = G1;
            if (SIGMA[class][model] < 1200 && SIGMA[class][model] >= 192)
                Gfactor_in_dead[class][model] = G2;
            if (SIGMA[class][model] < 192 && SIGMA[class][model] >= 96)
                Gfactor_in_dead[class][model] = G3;
            if (SIGMA[class][model] < 96 && SIGMA[class][model] >= 48)
                Gfactor_in_dead[class][model] = G4;
            if (SIGMA[class][model] < 48 && SIGMA[class][model] >= 16)
                Gfactor_in_dead[class][model] = G5;
            if (SIGMA[class][model] < 16)
                Gfactor_in_dead[class][model] = 0.0;
        }

        /* Ffactor_dead[model] =
            class_sum / (class_sum + WO[3][model] * SIGMA[3][model]); */
        /* Ffactor_live[model] = 1 - Ffactor_dead[model]; */
    }

    /*if considering spotting, read elevation map into an array */
    if (spotting)
        for (row = 0; row < nrows; row++) {
            Rast_get_c_row(elev_fd, elev, row);
            for (col = 0; col < ncols; col++)
                DATA(map_elev, row, col) = elev[col];
        }

    /*major computation: compute ROSs one cell a time */
    for (row = 0; row < nrows; row++) {
        G_percent(row, nrows, 2);
        Rast_get_c_row(fuel_fd, fuel, row);
        if (parm.mois_1h->answer)
            Rast_get_c_row(mois_1h_fd, mois_1h, row);
        if (parm.mois_10h->answer)
            Rast_get_c_row(mois_10h_fd, mois_10h, row);
        if (parm.mois_100h->answer)
            Rast_get_c_row(mois_100h_fd, mois_100h, row);
        Rast_get_c_row(mois_live_fd, mois_live, row);
        if (parm.vel->answer)
            Rast_get_c_row(vel_fd, vel, row);
        if (parm.dir->answer)
            Rast_get_c_row(dir_fd, dir, row);
        if (parm.slope->answer)
            Rast_get_c_row(slope_fd, slope, row);
        if (parm.aspect->answer)
            Rast_get_c_row(aspect_fd, aspect, row);

        /*initialize cell buffers for output map layers */
        for (col = 0; col < ncols; col++) {
            base[col] = max[col] = maxdir[col] = 0;
            if (spotting)
                spotdist[col] = 0;
        }

        for (col = 0; col < ncols; col++) {
            /*check if a fuel is within the numCategories models,
             *if not, no processing; useful when no data presents*/
            if (fuel[col] < 1 || fuel[col] > numCategories)
                continue;
            if (parm.mois_1h->answer)
                moisture[0] = 0.01 * mois_1h[col];
            if (parm.mois_10h->answer)
                moisture[1] = 0.01 * mois_10h[col];
            if (parm.mois_100h->answer)
                moisture[2] = 0.01 * mois_100h[col];
            moisture[3] = 0.01 * mois_live[col];
            if (parm.aspect->answer)
                aspect[col] = (630 - aspect[col]) % 360;

            /* assign some dead fuel moisture if not completely
             *     given based on empirical relationship*/
            if (!(parm.mois_10h->answer || parm.mois_100h->answer)) {
                moisture[1] = moisture[0] + 0.01;
                moisture[2] = moisture[0] + 0.02;
            }
            if (!(parm.mois_1h->answer || parm.mois_100h->answer)) {
                moisture[0] = moisture[1] - 0.01;
                moisture[2] = moisture[1] + 0.01;
            }
            if (!(parm.mois_1h->answer || parm.mois_10h->answer)) {
                moisture[0] = moisture[2] - 0.02;
                moisture[1] = moisture[2] - 0.01;
            }
            if (!(parm.mois_1h->answer) && parm.mois_10h->answer &&
                parm.mois_100h->answer)
                moisture[0] = moisture[1] - 0.01;
            if (!(parm.mois_10h->answer) && parm.mois_1h->answer &&
                parm.mois_100h->answer)
                moisture[1] = moisture[0] + 0.01;
            if (!(parm.mois_100h->answer) && parm.mois_1h->answer &&
                parm.mois_10h->answer)
                moisture[2] = moisture[1] + 0.01;

            /*compute xmext, moisture of extinction of live fuels */
            wmfd = 0.0;
            fined = 0.0;
            if (SIGMA[3][fuel[col]] > 0.0) {
                for (class = 0; class <= 2; class ++) {
                    if (SIGMA[class][fuel[col]] == 0.0)
                        continue;
                    fined = fined + WO[class][fuel[col]] *
                                        exp(-138.0 / SIGMA[class][fuel[col]]);
                    wmfd = wmfd + WO[class][fuel[col]] *
                                      exp(-138.0 / SIGMA[class][fuel[col]]) *
                                      moisture[class];
                }
                fdmois = wmfd / fined;
                finel = WO[3][fuel[col]] * exp(-500.0 / SIGMA[3][fuel[col]]);
                xmext = 2.9 * (fined / finel) * (1 - fdmois / MX[fuel[col]]) -
                        0.226;
            }
            else
                xmext = MX[fuel[col]];
            if (xmext < MX[fuel[col]])
                xmext = MX[fuel[col]];

            /*compute other intermediate values */
            Mf_dead = 0.0;
            wn_dead = 0.0;
            class_sum = 0.0;
            for (class = 0; class <= 2; class ++) {
                Mf_dead = Mf_dead +
                          moisture[class] * Ffactor_in_dead[class][fuel[col]];
                wn_dead = wn_dead + WO[class][fuel[col]] *
                                        Gfactor_in_dead[class][fuel[col]] *
                                        (1 - ST);
                Qig[class] = 250 + 1116 * moisture[class];
                class_sum = class_sum + Ffactor_all[class][fuel[col]] *
                                            epsilon[class][fuel[col]] *
                                            Qig[class];
            }
            etaM_dead =
                1.0 - 2.59 * (Mf_dead / MX[fuel[col]]) +
                5.11 * (Mf_dead / MX[fuel[col]]) * (Mf_dead / MX[fuel[col]]) -
                3.52 * (Mf_dead / MX[fuel[col]]) * (Mf_dead / MX[fuel[col]]) *
                    (Mf_dead / MX[fuel[col]]);
            if (Mf_dead >= MX[fuel[col]])
                etaM_dead = 0.0;
            etaM_live = 1.0 - 2.59 * (moisture[3] / xmext) +
                        5.11 * (moisture[3] / xmext) * (moisture[3] / xmext) -
                        3.52 * (moisture[3] / xmext) * (moisture[3] / xmext) *
                            (moisture[3] / xmext);
            if (moisture[3] >= xmext)
                etaM_live = 0.0;
            wn_live = WO[3][fuel[col]] * (1 - ST);
            Qig[3] = 250 + 1116 * moisture[3];
            class_sum = class_sum + Ffactor_all[3][fuel[col]] *
                                        epsilon[3][fuel[col]] * Qig[3];

            /*final computations */
            rhob = (wo_dead[fuel[col]] + WO[3][fuel[col]]) / DELTA[fuel[col]];
            beta = rhob / rhop;
            betaop = 3.348 / pow(sigma[fuel[col]], 0.8189);
            A = 133 / pow(sigma[fuel[col]], 0.7913);
            gammamax = pow(sigma[fuel[col]], 1.5) /
                       (495 + 0.0594 * pow(sigma[fuel[col]], 1.5));
            gamma =
                gammamax * pow(beta / betaop, A) * exp(A * (1 - beta / betaop));
            xi = exp((0.792 + 0.681 * pow(sigma[fuel[col]], 0.5)) *
                     (beta + 0.1)) /
                 (192 + 0.2595 * sigma[fuel[col]]);
            IR = gamma * h * (wn_dead * etaM_dead + wn_live * etaM_live) * etas;

            R0 = IR * xi / (rhob * class_sum);

            if (parm.vel->answer && parm.dir->answer) {
                C = 7.47 * exp(-0.133 * pow(sigma[fuel[col]], 0.55));
                B = 0.02526 * pow(sigma[fuel[col]], 0.54);
                E = 0.715 * exp(-0.000359 * sigma[fuel[col]]);
                phiw = C * pow((double)vel[col], B) / pow(beta / betaop, E);
            }
            else
                phiw = 0.0;

            if (parm.slope->answer && parm.aspect->answer) {
                phis = 5.275 * pow(beta, -0.3) * tan(M_D2R * slope[col]) *
                       tan(M_D2R * slope[col]);
            }
            else
                phis = 0.0;

            /*compute the maximum ROS R and its direction,
             *vector adding for the effect of wind and slope*/
            if (parm.dir->answer && parm.aspect->answer) {
                sin_fac = phiw * sin(M_D2R * dir[col]) +
                          phis * sin(M_D2R * aspect[col]);
                cos_fac = phiw * cos(M_D2R * dir[col]) +
                          phis * cos(M_D2R * aspect[col]);
                phi_ws = sqrt(sin_fac * sin_fac + cos_fac * cos_fac);
                Rdir = atan2(sin_fac, cos_fac) / M_D2R;
            }
            else if (parm.dir->answer && !(parm.aspect->answer)) {
                phi_ws = phiw;
                Rdir = dir[col];
            }
            else if (!(parm.dir->answer) && parm.aspect->answer) {
                phi_ws = phis;
                Rdir = (float)aspect[col];
            }
            else {
                phi_ws = 0.0;
                Rdir = 0.0;
            }
            R = R0 * (1 + phi_ws);
            if (Rdir < 0.0)
                Rdir = Rdir + 360;
            /*printf("\nparm.dir->aanswer=%s, parm.aspect->aanswer=%s, phis=%f,
             * phi_ws=%f,
             * aspect[col]=%d,Rdir=%f",parm.dir->answer,parm.aspect->answer,phis,phi_ws,aspect[col],Rdir);
             */

            /*maximum spotting distance */
            if (spotting)
                spotdist[col] =
                    spot_dist(fuel[col], R, vel[col], Rdir, row, col);

            /*to avoid 0 R, convert ft./min to cm/min */
            R0 = 30.5 * R0;
            R = 30.5 * R;
            /*4debugging            R0 = R0/30.5/1.1; R = R/30.5/1.1; */

            base[col] = (int)R0;
            max[col] = (int)R;
            maxdir[col] = (int)Rdir;
            /*printf("(%d, %d)\nR0=%.2f, vel=%d, dir=%d, phiw=%.2f, s=%d, as=%d,
             * phis=%.2f, R=%.1f, Rdir=%.0f\n", row, col, R0, vel[col],
             * dir[col], phiw, slope[col], aspect[col], phis, R, Rdir); */
        }
        Rast_put_row(base_fd, base, CELL_TYPE);
        Rast_put_row(max_fd, max, CELL_TYPE);
        Rast_put_row(maxdir_fd, maxdir, CELL_TYPE);
        if (spotting)
            Rast_put_row(spotdist_fd, spotdist, CELL_TYPE);
    }
    G_percent(row, nrows, 2);

    Rast_close(fuel_fd);
    if (parm.mois_1h->answer)
        Rast_close(mois_1h_fd);
    if (parm.mois_10h->answer)
        Rast_close(mois_10h_fd);
    if (parm.mois_100h->answer)
        Rast_close(mois_100h_fd);
    Rast_close(mois_live_fd);
    if (parm.vel->answer)
        Rast_close(vel_fd);
    if (parm.dir->answer)
        Rast_close(dir_fd);
    if (parm.slope->answer)
        Rast_close(slope_fd);
    if (parm.aspect->answer)
        Rast_close(aspect_fd);
    Rast_close(base_fd);
    Rast_close(max_fd);
    Rast_close(maxdir_fd);
    if (spotting) {
        Rast_close(spotdist_fd);
        G_free(map_elev);
    }

    /*
       for (model = 1; model <= 13; model++) {
       if (model == 1)
       printf("\n           Grass and Grass-dominated\n");
       else if (model == 4)
       printf("             Chaparral and Shrubfields\n");
       else if (model == 8)
       printf("                   Timber Litter\n");
       else if (model == 11)
       printf("                   Logging Slash\n");
       printf("Model %2d   ", model);
       for (class = 0; class <= 3; class++)
       printf("%4.0f/%.3f ", SIGMA[class][model], WO[class][model]);
       printf("  %.1f  %.2f\n", DELTA[model], MX[model]);
       }

       printf("\nWeight in All Fuel Subclasses:\n");
       for (model = 1; model <= 13; model++) {
       printf("model %2d  ", model);
       for (class = 0; class <= 3; class++)
       printf("%.2f  ", Ffactor_all[class][model]);
       printf("%4.0f/%.3f=%6.0f  model %2d\n", sigma[model],
       wo_dead[model]+WO[3][model], sigma[model]/(wo_dead[model]+WO[3][model]),
       model);
       }
       printf("\nWeight in Dead Fuel Subclasses, Dead Weight/Live
       Weight:\n");for (model = 1; model <= 13; model++) { printf("model %2d  ",
       model); for (class = 0; class <= 2; class++) printf("%.2f  ",
       Ffactor_in_dead[class][model]); printf("%.2f/%.2f  model %2d\n",
       Ffactor_dead[model], Ffactor_live[model], model);
       }
     */

    G_done_msg(_("Raster maps <%s>, <%s> and <%s> created."), name_base,
               name_max, name_maxdir);

    exit(EXIT_SUCCESS);
}
