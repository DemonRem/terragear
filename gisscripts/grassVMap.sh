#!/bin/bash
#
# Written by Martin Spott
#
# Copyright (C) 2010  Markus Metz @ GRASS GIS
# Copyright (C) 2010 - 2013  Martin Spott - Martin (at) flightgear (dot) org
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

date

LOADDIR=${HOME}/live/vmap0shp
DUMPDIR=${HOME}/shp
mkdir -p ${DUMPDIR}
RUNDIR=`pwd`
cd `dirname ${0}` && export BASEDIR=`pwd`
cd ${RUNDIR}
#
MAPPINGFILE=${BASEDIR}/CORINEtoCS.txt

#SNAP=0.00001
SNAP=0.000001
#MIN_AREA1=1
MIN_AREA1=0.0000001
MIN_AREA2=100
#MIN_AREA2=0.000000001

PREFIX=v0
CLEANMAP=worldcopy

# DB-Connection
PGHOST=geoscope.optiputer.net
PGDATABASE=landcover
PGUSER=martin
DSN="PG:host=${PGHOST} dbname=${PGDATABASE} user=${PGUSER}"
LAYEROPTS="FID=ogc_fid, GEOMETRY_NAME=wkb_geometry, SPATIAL_INDEX=YES, PRIMARY_KEY=YES, SRID=4326"
PSQL="psql -tA -h ${PGHOST} -U ${PGUSER} -d ${PGDATABASE}"

########################################################################

fn_import() {
    for SHAPEFILE in `ls ${LOADDIR}/*-area.shp`; do
        LAYER=`basename ${SHAPEFILE} | cut -f 1 -d \.`
        MAP=`echo ${LAYER} | sed -e 's/-/_/g'`
        g.remove vect=${MAP}
        v.in.ogr dsn="${LOADDIR}" layer=${LAYER} output=${MAP} snap=${SNAP} --verbose
    done
}

########################################################################

# v.extract input=soa_veg_grassa_area type=area output=v0_grassland where="f_code LIKE 'EB010'"
#
#       sourcelayer        |    sourceattrs     |        pgislayer
#--------------------------+--------------------+--------------------------
# 'builtupa@pop(*)_area'   |                    | v0_urban
# 'cropa@veg(*)_area'      | veg=0              | v0_mixedcrop
# 'cropa@veg(*)_area'      | veg=1              | v0_drycrop
# 'cropa@veg(*)_area'      | veg=999            | v0_irrcrop
# 'grassa@veg(*)_area'     | f_code=EB010       | v0_grassland
# 'grassa@veg(*)_area'     | f_code=EB020       | v0_scrub
# 'grounda@phys(*)_area'   | smc=119            | v0_barrencover
# 'grounda@phys(*)_area'   | smc=52             | v0_lava
# 'grounda@phys(*)_area'   | smc=88:swc=0       | v0_sand
# 'grounda@phys(*)_area'   | smc=88:swc=3       | v0_littoral
# 'inwatera@hydro(*)_area' | f_code=BH000:hyc=6 | v0_intermittentlake
# 'inwatera@hydro(*)_area' | f_code=BH000:hyc=8 | v0_lake
# 'inwatera@hydro(*)_area' | f_code=BH090       | v0_floodland
# 'landicea@phys(*)_area'  |                    | v0_glacier
# 'polbnda@bnd(*)_area'    |                    | v0_landmass
# 'seaicea@phys(*)_area'   | f_code=BJ070       | v0_packice
# 'seaicea@phys(*)_area'   | f_code=BJ080       | v0_polarice
# 'swampa@veg(*)_area'     | f_code=BH015       | v0_bog
# 'swampa@veg(*)_area'     | f_code=BH095       | v0_marsh
# 'treesa@veg(*)_area'     | veg=24             | v0_deciduousforest
# 'treesa@veg(*)_area'     | veg=25             | v0_evergreenforest
# 'treesa@veg(*)_area'     | veg=50             | v0_mixedforest
# 'tundraa@veg(*)_area'    |                    | v0_herbtundra

fn_split() {
    g.remove vect=`g.mlist type=vect pattern="${PREFIX}_*_???" separator=,`

    for CLASS in `g.mlist type=vect pattern="???_*_*_area" | cut -f 2,3 -d \_ | sort | uniq`; do  # veg_cropa, veg_grassa
        for LAYER in `g.mlist type=vect pattern="???_${CLASS}_area" | sort`; do  # eur_veg_cropa_area, noa_veg_cropa_area
            ZONE=`echo ${LAYER} | cut -f 1 -d \_`  # eur, noa
            V0LAYER=`echo ${LAYER} | awk -F\_ '{print $3 "@" $2 "(*)_" $4}'`  # cropa@veg(*)_area
            case ${V0LAYER} in
                "polbnda@bnd(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_landmass_${ZONE}
                ;;
                "inwatera@hydro(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_intermittentlake_${ZONE} where="f_code LIKE 'BH000' AND hyc != 8"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_lake_${ZONE} where="f_code LIKE 'BH000' AND hyc = 8"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_floodland_${ZONE} where="f_code NOT LIKE 'BH000'"
                ;;
                "extracta@ind(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_saline_${ZONE} where="f_code LIKE 'BH155'"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_openmining_${ZONE} where="f_code NOT LIKE 'BH155'"
                ;;
                "fishinda@ind(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_fishing_${ZONE}
                ;;
                "grounda@phys(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_barrencover_${ZONE} where="smc != 52 AND smc != 88"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_lava_${ZONE} where="smc = 52"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_littoral_${ZONE} where="smc = 88 AND swc = 3"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_sand_${ZONE} where="smc = 88 AND swc != 3"
                ;;
                "landicea@phys(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_glacier_${ZONE}
                ;;
                "seaicea@phys(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_packice_${ZONE} where="f_code NOT LIKE 'BJ080'"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_polarice_${ZONE} where="f_code LIKE 'BJ080'"
                ;;
                "builtupa@pop(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_urban_${ZONE}
                ;;
                "mispopa@pop(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_suburban_${ZONE}
                ;;
                "cropa@veg(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_mixedcrop_${ZONE} where="f_code NOT LIKE 'BH135' and veg = 0"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_drycrop_${ZONE} where="f_code NOT LIKE 'BH135' and veg = 1"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_irrcrop_${ZONE} where="f_code NOT LIKE 'BH135' and veg > 1"
#                    v.extract -t input=${LAYER} type=area output=${PREFIX}_rice_${ZONE} where="f_code LIKE 'BH135'"
                ;;
                "grassa@veg(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_grassland_${ZONE} where="f_code LIKE 'EB010'"
#                    v.extract -t input=${LAYER} type=area output=${PREFIX}_bamboo_${ZONE} where="f_code LIKE 'EC010'"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_scrub_${ZONE} where="f_code NOT LIKE 'EB010' AND f_code NOT LIKE 'EC010'"
                ;;
                "oasisa@veg(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_oasis_${ZONE}
                ;;
                "orcharda@veg(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_vineyard_${ZONE} where="f_code LIKE 'EA050'"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_orchard_${ZONE} where="f_code NOT LIKE 'EA050'"
                ;;
                "swampa@veg(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_bog_${ZONE} where="f_code LIKE 'BH015'"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_marsh_${ZONE} where="f_code NOT LIKE 'BH015'"
                ;;
                "treesa@veg(*)_area")
#                    v.extract -t input=${LAYER} type=area output=${PREFIX}_mangrove_${ZONE} where="veg = 19"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_deciduousforest_${ZONE} where="veg = 24"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_evergreenforest_${ZONE} where="veg = 25"
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_mixedforest_${ZONE} where="veg != 19 AND veg != 24 AND veg != 25"
                ;;
                "tundraa@veg(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_herbtundra_${ZONE}
                ;;
                "vegvoida@veg(*)_area")
                    v.extract -t input=${LAYER} type=area output=${PREFIX}_vegvoid_${ZONE}
                ;;
            esac
        done
    done
}

########################################################################

fn_reclass() {
    for OUTPUT in `g.mlist type=vect pattern="${PREFIX}_*_???" | awk -F\_ '{print $1 "_" $2}' | sort | uniq`; do
        CATEGORY=`echo ${OUTPUT} | sed -e "s/^${PREFIX}_/cs_/g"`
        CODECLC=`grep "\ ${CATEGORY}\$" ${MAPPINGFILE} | awk '{print $1}'`
        for ZONE in `g.mlist type=vect pattern="${PREFIX}_*_[a-z][a-z][a-z]" | awk -F\_ '{print $3}' | sort | uniq`; do
            LCCMAP=${OUTPUT}_${ZONE}_lcclass
            g.remove vect=${LCCMAP}
            v.db.addtable map=${OUTPUT}_${ZONE}
            v.db.addcolumn map=${OUTPUT}_${ZONE} columns="newcodeCLC integer" --verbose
            v.db.update map=${OUTPUT}_${ZONE} column=newcodeCLC value=${CODECLC} --verbose
            v.db.dropcolumn map=${OUTPUT}_${ZONE} column=code_CLC --verbose
            v.db.renamecolumn map=${OUTPUT}_${ZONE} column=newcodeCLC,code_CLC --verbose
            v.reclass input=${OUTPUT}_${ZONE} output=${LCCMAP} column=code_CLC --verbose
        done
#        g.remove vect=${OUTPUT}_patched
        v.patch input=`g.mlist type=vect pattern="${OUTPUT}_[a-z][a-z][a-z]_lcclass" separator=,` output=${OUTPUT}_patched
        g.remove vect=${OUTPUT}_bpol,${OUTPUT}_snap,${OUTPUT}_split,${OUTPUT}_rmsa,${OUTPUT}_rmdangle,${OUTPUT}_rmarea,${OUTPUT}_prune,${OUTPUT}_polyline,${OUTPUT}_dissolved
#
        v.clean input=${OUTPUT}_patched output=${OUTPUT}_bpol -c tool=bpol type=boundary --verbose
        v.clean input=${OUTPUT}_bpol output=${OUTPUT}_snap -c tool=snap thresh=${SNAP} type=boundary --verbose
        v.split input=${OUTPUT}_snap output=${OUTPUT}_split length=40 units=kilometers --verbose
        v.clean input=${OUTPUT}_split output=${OUTPUT}_rmsa -c tool=rmsa type=boundary --verbose
        v.clean input=${OUTPUT}_rmsa output=${OUTPUT}_rmdangle tool=rmline,rmdangle thresh=0,-1 type=boundary --verbose
        date
        v.clean input=${OUTPUT}_rmdangle output=${OUTPUT}_rmarea tool=rmarea thresh=${MIN_AREA1} type=boundary --verbose
        date
        v.clean input=${OUTPUT}_rmarea output=${OUTPUT}_prune tool=prune thresh=0.00001 type=boundary --verbose
        v.build.polylines input=${OUTPUT}_prune output=${OUTPUT}_polyline --verbose
        v.dissolve input=${OUTPUT}_polyline output=${OUTPUT}_dissolved --verbose
        v.out.ogr input=${OUTPUT}_dissolved type=area dsn=${DUMPDIR}/${OUTPUT}_pre-clip.shp
    done
}

########################################################################

fn_fixpostgis() {
    LAYER=${1}
    echo "DROP INDEX ${LAYER}_cat_idx;"
    echo "ALTER INDEX ${LAYER}_pkey RENAME TO ${LAYER}_pk;"
    echo "ALTER INDEX ${LAYER}_wkb_geometry_idx RENAME TO ${LAYER}_gindex;"
    echo "CLUSTER ${LAYER}_gindex ON ${LAYER};"
    echo "ALTER TABLE ${LAYER} ADD CONSTRAINT "enforce_dims_wkb_geometry" CHECK (ST_NDims(wkb_geometry) = 2);"
    echo "ALTER TABLE ${LAYER} ADD CONSTRAINT "enforce_geotype_wkb_geometry" CHECK (GeometryType(wkb_geometry) = 'POLYGON'::text);"
    echo "ALTER TABLE ${LAYER} ADD CONSTRAINT "enforce_srid_wkb_geometry" CHECK (ST_SRID(wkb_geometry) = 4326);"
    echo "ALTER TABLE ${LAYER} DROP COLUMN id;"
    echo "ALTER TABLE ${LAYER} ADD COLUMN src_id numeric(5,0) DEFAULT 1;"
    echo "ALTER TABLE ${LAYER} ADD COLUMN maint_id numeric(5,0) DEFAULT NULL;"
    echo "ALTER TABLE ${LAYER} ADD COLUMN ch_date timestamp;"
    echo "GRANT SELECT ON ${LAYER} TO webuser;"
    echo "VACUUM ANALYZE ${LAYER};"
}

fn_topostgis() {
    GRMAP=${1}
    PGLAYER=${PREFIX}_`echo ${GRMAP} | cut -f 2 -d \_`
    echo "DROP TABLE ${PGLAYER};" | ${PSQL}
    v.out.postgis input=${GRMAP} olayer=${PGLAYER} dsn="${DSN}" options="${LAYEROPTS}"
    fn_fixpostgis ${PGLAYER} | ${PSQL}
}

###############################################################################

fn_overlay() {
    COUNT=1
    for LAYER in ${PREFIX}_lake \
                 ${PREFIX}_intermittentlake \
                 ${PREFIX}_floodland \
                 ${PREFIX}_glacier \
                 ${PREFIX}_packice \
                 ${PREFIX}_polarice \
                 ${PREFIX}_urban \
                 ${PREFIX}_suburban \
                 ${PREFIX}_fishing \
                 ${PREFIX}_bog \
                 ${PREFIX}_marsh \
                 ${PREFIX}_saline \
                 ${PREFIX}_openmining \
                 ${PREFIX}_drycrop \
                 ${PREFIX}_irrcrop \
                 ${PREFIX}_mixedcrop \
                 ${PREFIX}_grassland \
                 ${PREFIX}_scrub \
                 ${PREFIX}_deciduousforest \
                 ${PREFIX}_evergreenforest \
                 ${PREFIX}_mixedforest \
                 ${PREFIX}_herbtundra \
                 ${PREFIX}_littoral \
                 ${PREFIX}_barrencover \
                 ${PREFIX}_sand \
                 ${PREFIX}_lava \
                 ${PREFIX}_landmass;
    do
    v.info map=${LAYER} > /dev/null
    LAYERVALID=${?}
    if [ ${LAYERVALID} -eq 0 ]; then
            g.remove vect=${LAYER}_postsplit
            v.split input=${LAYER}_dissolved output=${LAYER}_postsplit length=40 units=kilometers --verbose
            v.out.ogr input=${LAYER}_postsplit type=area dsn=${DUMPDIR}/${LAYER}_post-split.shp
            if [ ${COUNT} -eq 1 ]; then
                g.remove vect=${CLEANMAP}
                g.copy vect=${LAYER}_postsplit,${CLEANMAP}
            else
                g.remove vect=tmp
                v.overlay ainput=${CLEANMAP} binput=${LAYER}_postsplit output=tmp operator=or snap=0.000001
                v.db.addcolumn map=tmp columns="newcat integer" --verbose
                v.db.update map=tmp column=newcat value=b_cat where="a_cat IS NULL" --verbose
                v.db.update map=tmp column=newcat value=a_cat where="b_cat IS NULL" --verbose
                v.db.update map=tmp column=newcat value=a_cat where="newcat IS NULL" --verbose
                v.db.update map=tmp column=newcat value=a_cat where="newcat=0" --verbose
                g.remove vect=${CLEANMAP}_reclass
                v.reclass input=tmp output=${CLEANMAP}_reclass column=newcat --verbose
                g.remove vect=${CLEANMAP}
                g.rename vect=${CLEANMAP}_reclass,${CLEANMAP}
            fi
            COUNT=`expr ${COUNT} + 1`
    fi
    done
}

fn_preexport() {
    SELECTION=`v.category input=${CLEANMAP} type=centroid option=print | sort -n | uniq`
    for CATEGORY in ${SELECTION}; do
        LAYER=`grep \^${CATEGORY} ${MAPPINGFILE} | awk '{print $2}' | sed -e "s/^cs_/${PREFIX}_/g"`
        g.remove vect=${LAYER}_postclip
        v.extract cats=${CATEGORY} input=${CLEANMAP} output=${LAYER}_postclip type=area
        v.out.ogr input=${LAYER}_postclip type=area dsn=${DUMPDIR}/${LAYER}_post-clip.shp
    done
}

###############################################################################

fn_clean() {
    # Caution, don't overwrite the only functional working copy while testing !!!
    g.remove vect=${PREFIX}_patched,${PREFIX}_bpol,${PREFIX}_snap,${PREFIX}_rmsa,${PREFIX}_rmdangle,${PREFIX}_rmarea,${PREFIX}_prune,${PREFIX}_polyline,${PREFIX}_dissolved
    g.rename vect=${CLEANMAP},${PREFIX}_patched
    #
    v.clean input=${PREFIX}_patched output=${PREFIX}_bpol -c tool=bpol type=boundary --verbose
    v.clean input=${PREFIX}_bpol output=${PREFIX}_snap -c tool=snap thresh=${SNAP} type=boundary --verbose
    v.clean input=${PREFIX}_snap output=${PREFIX}_rmsa -c tool=rmsa type=boundary --verbose
    v.clean input=${PREFIX}_rmsa output=${PREFIX}_rmdangle tool=rmline,rmdangle thresh=0,-1 type=boundary --verbose
    date
    v.clean input=${PREFIX}_rmdangle output=${PREFIX}_rmarea tool=rmarea thresh=${MIN_AREA2} type=boundary --verbose
    date
    v.clean input=${PREFIX}_rmarea output=${PREFIX}_prune tool=prune thresh=0.00001 type=boundary --verbose
    v.build.polylines input=${PREFIX}_prune output=${PREFIX}_polyline --verbose
    v.dissolve input=${PREFIX}_polyline output=${PREFIX}_dissolved --verbose

    #v.split input=${PREFIX}_patched output=${PREFIX}_split layer=-1 vertices=100 --verbose
    #
    #oder kein v.split, kein v.clean tool=bpol, kein v.clean tool=snap, sondern
    #
    #v.clean input=${PREFIX}_patched output=${PREFIX}_rmarea tool=rmarea thresh=100
}

###############################################################################

fn_proj() {
    # Re-project from EPSG:3035 - this one is _not_ to be run inside
    # the import location !!!
    MYLOCATION=`g.gisenv get=LOCATION_NAME`
    MYMAPSET=`g.gisenv get=MAPSET`
    g.mapset location=wgs84 mapset=${MYMAPSET}
    g.remove vect=${CLEANMAP}
    v.proj location=${MYLOCATION} mapset=${MYMAPSET} input=${CLEANMAP}  # output=${CLEANMAP}
    if [ ${PREFIX} = "v0" ]; then
        g.remove vect=${PREFIX}_landmass_polyline
        v.proj location=${MYLOCATION} mapset=${MYMAPSET} input=${PREFIX}_landmass_polyline
    fi
}

###############################################################################

fn_export() {
    # Print available (integer) categories - centroids only !!!
    #
    SELECTION=`v.category input=${CLEANMAP} type=centroid option=print | sort -n | uniq`
    for CATEGORY in ${SELECTION}; do
        LAYER=`grep \^${CATEGORY} ${MAPPINGFILE} | awk '{print $2}' | sed -e "s/^cs_/${PREFIX}_/g"`
        g.remove vect=${LAYER}
        v.extract cats=${CATEGORY} input=${CLEANMAP} output=${LAYER} type=area
        if [ ${LAYER} = "${PREFIX}_landmass" ]; then
            NEWLAYER="${PREFIX}_void"
            g.remove vect=${NEWLAYER}
            g.rename vect=${LAYER},${NEWLAYER}
            LAYER=${NEWLAYER}
        fi
#        v.out.ogr input=${LAYER} type=area dsn=${DUMPDIR}/${LAYER}.shp
        fn_topostgis ${LAYER}
    done
    if [ ${PREFIX} = "v0" ]; then
#        v.out.ogr input=${PREFIX}_landmass_polyline type=area dsn=${DUMPDIR}/${PREFIX}_landmass.shp
        fn_topostgis ${PREFIX}_landmass_polyline
    fi
}

###############################################################################

fn_import
fn_split
fn_reclass
#
fn_overlay
fn_preexport
fn_clean
CLEANMAP=${PREFIX}_dissolved
fn_proj
fn_export

# EOF
