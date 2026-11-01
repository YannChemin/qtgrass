# GRASS Addons Menu Placement

This file is the **authoritative source** for where every grass-addon module
appears in the qtgrass menus.  Edit the **Menu Path** column to move a module.

* `Menu Path` uses `/`-separated lowercase slugs matching the Qt menu hierarchy.
  Example: `imagery/evapotranspiration` → Imagery → Evapotranspiration submenu.
* Rows marked `⚠️ NEW` were auto-detected and need a human to confirm placement.
* Re-run cmake after editing this file to regenerate the C++ header.

| Module | Menu Path | Description |
|--------|-----------|-------------|
| `d.explanation.plot` | settings/explanation | D.explantion.plot creates a plot of rasters and their relations |
| `d.frame` | settings/frame | Manages display frames on the current user's graphics monitor |
| `d.mon2` | after:d.mon | Extended display monitor (experimental successor to d.mon) |
| `d.region.grid` | settings/region | Plots a grid defined by the computational region or |
| `d.vect.thematic2` | after:d.vect.thematic | Creates thematic vector maps (extended d.vect.thematic with more options) |
| `db.join` | database/join | Joins the content of one attribute table into another |
| `g.citation` | settings/citation | Creates citation or metadata based on documentation |
| `g.compare.md5` | settings/compare | Checks if two GRASS maps are identical |
| `g.copyall` | settings/copyall | Copies maps/files of a specified from a selected mapset |
| `g.download.location` | settings/download | Downloads a GRASS Location archive (zip/tar.gz) from the web and unpacks it |
| `g.isis3mt` | settings/isis3mt | Generates an ISIS3 map template file according |
| `g.proj.all` | settings/map_projections | Reprojects all raster and vector maps from given location |
| `g.proj.identify` | settings/map_projections | Allows to automaticaly identify the EPSG code |
| `g.projpicker` | settings/projpicker | Queries projections spatially. It is a wrapper |
| `g.rename.many` | file/manage_maps | Renames multiple maps at once |
| `i.ai` | imagery/ai | A native GRASS GIS addon that integrates Ollama LLM with GRASS GIS for comprehen |
| `i.ann.maskrcnn` | imagery/neural-networks | .train . The user feeds the module with training data consisting of images and masks for each instance of intended |
| `i.atcorr2` | after:i.atcorr | Performs atmospheric correction of single-band raster (extended i.atcorr) |
| `i.basemap` | imagery/basemap | Provides a comprehensive interface for downloading and importing ba |
| `i.cutlines` | imagery/cutlines | Tiles the images into tiles with irregular borders |
| `i.cva` | imagery/cva | Calculates Change Vector Analysis (CVA) for two input variables |
| `i.destripe` | imagery/destripe | Destripes in a row-based Fourier smoothing way. Requires |
| `i.eb.deltat` | imagery/evapotranspiration_calculation | Calculates the difference of temperature between two |
| `i.eb.hsebal95` | imagery/evapotranspiration_calculation | Performs sensible heat flux iteration using the SEBAL 95 energy balance method |
| `i.eb.z0m` | imagery/evapotranspiration_calculation | Calculates the momentum roughness length (z0m) and optionally |
| `i.eb.z0m0` | after:i.eb.z0m | Calculates momentum roughness length z0m (variant of i.eb.z0m) |
| `i.edge` | imagery/edge | Checks if an edge detector based on the Canny edge detection algorithm |
| `i.eodag` | imagery/eodag | Allows to search and download imagery scenes, e.g. Sentinel |
| `i.evapo.potrad` | imagery/evapotranspiration_calculation | Calculates the diurnal potential evapotranspiration |
| `i.evapo.senay` | imagery/evapotranspiration_calculation | Calculates the diurnal actual evapotranspiration |
| `i.evapo.zk` | imagery/evapotranspiration_calculation | Calculates the global diurnal evapotranspiration |
| `i.feotio2` | imagery/feotio2 | I.feo\tio2 calculates the FeO or TiO2 contents from Clementine data |
| `i.fusion.hpf` | imagery/image-fusion | Checks if an implementation of the High Pass Filter Additive |
| `i.gabor` | imagery/gabor | Computes texture raster maps from a user-specified Gabor |
| `i.gcp` | imagery/gcp | Manages Ground Control Points (GCPs) non-interactively |
| `i.gravity` | imagery/gravity | Calculates the Bouguer gravity anomaly (after Mussett |
| `i.histo.match` | imagery/histograms | Performs histogram matching on the given input images |
| `i.hyper` | imagery/hyperspectral-analysis | Complete toolset for hyperspectral data: import, atmospheric correction, visualisation and export |
| `i.hyper.albedo` | imagery/hyperspectral-analysis | Calculates broadband albedo from hyperspectral imagery |
| `i.hyper.atcorr` | imagery/hyperspectral-analysis | NAME i.hyper.atcorr - Atmospheric correction of hyperspectral imagery using the 6SV2.1 radiative transfer algorithm |
| `i.hyper.continuum` | imagery/hyperspectral-analysis | Applies continuum removal to hyperspectral imagery |
| `i.hyper.geology` | imagery/hyperspectral-analysis | GRASS GIS manual DESCRIPTION i.hyper.geology performs geological rock family classification, mineralogical range |
| `i.hyper.indices` | imagery/hyperspectral-analysis | Calculates spectral indices from hyperspectral or multispectral imagery |
| `i.hyper.rgb` | imagery/hyperspectral-analysis | Creates RGB or CMYK composites from hyperspectral imagery |
| `i.hyper.smac` | imagery/hyperspectral-analysis | Applies SMAC atmospheric correction to hyperspectral imagery stored as 3D rasters |
| `i.hyper.specresamp` | imagery/hyperspectral-analysis | Performs spectral resampling of hyperspectral |
| `i.hyper.spectroscopy` | imagery/hyperspectral-analysis | GRASS GIS manual DESCRIPTION i.hyper.spectroscopy performs per-pixel spectral feature interpretation of hyperspectral |
| `i.image.bathymetry` | imagery/develop_images_and_groups | Checks if used to estimate Satellite-Derived Bathymetry |
| `i.in.probav` | file/import_raster_data | Imports Proba-V NDVI data sets. After the import |
| `i.landsat` | imagery/satellite_images_tools | Toolset for Landsat imagery: download (TM/ETM/OLI), atmospheric correction, and band analysis |
| `i.landsat8.swlst` | imagery/landsat8 | Checks if an implementation of a robust and practical |
| `i.lmf` | imagery/lmf | Calculates the Local maximum fitting of a temporal image series |
| `i.lswt` | imagery/lswt | Computes Lake Water Surface Temperature (LSWT) from moderate |
| `i.modis` | imagery/satellite_images_tools | Suite is a toolset to import MODIS (Moderate Resolution Imaging Spectroradiometer, flown on the two NASA spacecrafts |
| `i.nightlights.intercalibration` | imagery/night-time-lights | Checks if a GRASS GIS module performing |
| `i.ocean` | imagery/ocean | - GRASS GIS ≥ 8 |
| `i.ortho.corr` | imagery/ortho | Allows to correct orthophotos using the camera angle map |
| `i.points.auto` | imagery/points | Tries to automatically generate a given number of new |
| `i.pr` | imagery/pr | Pattern recognition toolset: k-NN, SVM, classification trees, and feature selection for imagery |
| `i.pysptools.unmix` | imagery/hyperspectral-pysptools | Extracts endmembers from imagery group and performs |
| `i.rh` | imagery/rh | Calculates the relative humidity -s will return saturated water |
| `i.rotate` | imagery/rotate | Rotates an input raster by a clockwise angle |
| `i.sam2` | imagery/sam2 | Allows users to segment orthoimagery based on text prompts |
| `i.sar.speckle` | imagery/sar-radar | A simple procedure to reduce speckle noise in SAR images |
| `i.segment.gsoc` | imagery/classify_image | Image segmentation is the process of grouping similar pixels into unique |
| `i.segment.hierarchical` | imagery/classify_image | Performs a hierarchical segmentation on a image |
| `i.segment.stats` | imagery/classify_image | Calculates statistics for areas in a raster map. Areas |
| `i.segment.uspo` | imagery/classify_image | Provides unsupervised segmentation parameter |
| `i.sentinel` | imagery/sentinel | Toolset consists of currently six modules: i.sentinel.coverage checks the area coverage of Sentinel-1 or Sentinel-2 |
| `i.signature.copy` | imagery/signature-management | Copies a signature file from a group/subgroup |
| `i.signature.list` | imagery/signature-management | Lists the name(s) of signature file(s) of a group |
| `i.spec.sam` | imagery/spectral-analysis | Checks if used to perform Spectral Angle Mapping |
| `i.spec.unmix` | imagery/spectral-analysis | Checks if used to perform Spectral Unmixing. The result is |
| `i.superpixels.slic` | imagery/super-pixels | Performs superpixel segmentation using a k means |
| `i.theilsen` | imagery/theilsen | A method for robust linear regression. Output is the median |
| `i.variance` | imagery/variance | Following the Woodcock and Strahler (1987), i |
| `i.vi.mpi` | imagery/satellite_images_products | Calculates different types of vegetation indices (mpi) |
| `i.water` | imagery/water | Calculates a water layer after Xiao et al (2005) and Roy et al |
| `i.wavelet` | imagery/wavelet | Decomposes the time-series with the requested wavelet |
| `i.wi` | imagery/wi | Calculates water indices based on biophysical parameters |
| `i.zero2null` | imagery/zero2null | Replaces zero cells with NULL cells at the edges |
| `m.cdo.download` | tools/cdo | Downloads climate records from NCEI's Climate Data Online (CDO) v2 API |
| `m.crawl.thredds` | tools/crawl | An increasing amount of spatio-temporal data, like climate observations |
| `m.csv.clean` | tools/csv | Reads a CSV (Comma Separated Value) file, cleans it |
| `m.eigensystem` | tools/eigensystem | Computes eigenvalues and eigenvectors of a square matrix supplied as a formatted text file |
| `m.gcp.filter` | tools/gcp | Filters GCPs using the distance between the computed |
| `m.printws` | tools/printws | Adds a quick 'print workspace' function to GRASS |
| `m.prism.download` | tools/prism | Downloads gridded climate data from the PRISM Climate Group (Oregon State) |
| `m.tnm.download` | tools/tnm | Downloads elevation, hydrography and other data from The National Map (TNM) |
| `r.accumulate` | raster/accumulate | Calculates weighted flow accumulation, subwatersheds |
| `r.agent` | raster/agent | Consists of a library libagent and some submodules which use this it. The submodules are described in their resp |
| `r.area` | raster/area-analysis | Values representing their size (in cells) |
| `r.area.createweight` | raster/area-analysis | Can be used to create a weighting layer |
| `r.basin` | raster/basin | Morphometric characterisation of river basins from DEM and flow direction rasters |
| `r.bearing.distance` | raster/bearing | Computes the bearing and/or straight-line distance |
| `r.bioclim` | raster/bioclim | Calculates various bioclimatic indices from monthly |
| `r.bitpattern` | raster/bitpattern | Performs bit pattern comparisons. The module can be used |
| `r.boxplot` | raster/boxplot | Draws boxplots of the raster values of an input raster map |
| `r.buildvrt.gdal` | raster/develop_raster_map | Builds GDAL virtual rasters over GRASS GIS raster maps |
| `r.catchment` | raster/catchment | Facilitates modeling "catchments" around |
| `r.category.trim` | raster/change_category_values_and_labels | In GRASS you can 'cut out' a subset of a larger raster layer by setting |
| `r.cell.area` | raster/cell | Uses the current computational region to compute the area |
| `r.centroids` | raster/centroids | Checks if a wrapper function |
| `r.change.info` | raster/change | Calculates landscape change assessment for a series |
| `r.clip` | raster/clip | Extracts the portion of a raster map that overlaps with the current computational region |
| `r.colors.contrastbrightness` | raster/manage_colors | Module generates a contrasted version |
| `r.colors.cubehelix` | raster/manage_colors | Module generates a cubehelix color table |
| `r.colors.matplotlib` | raster/manage_colors | Module converts |
| `r.colors.out_sld` | raster/manage_colors | The Styled Layer Description (SLD) format according to OGC standard |
| `r.colors.qml` | raster/manage_colors | Applies raster symbology defined in QGIS in a QML style file |
| `r.colors.toqml` | raster/manage_colors | Exports raster symbology defined in GRASS GIS to a QGIS QML |
| `r.composite` | raster/manage_colors | Combines three separate raster maps (red, green, blue |
| `r.confusionmatrix` | raster/confusionmatrix | Calculates the confusion matrix, overall, user |
| `r.connectivity` | raster/connectivity | Toolset for ecological network connectivity analysis: graph construction, centrality and corridors |
| `r.convergence` | raster/convergence | Increase convergence if slope value is high |
| `r.convert` | raster/format-conversion | Converts a GRASS 4-6 raster map to GRASS 7 raster map format |
| `r.cpt2grass` | raster/cpt2grass | Given raster map |
| `r.crater` | raster/crater | Program estimates the size of a gravity dominated impact |
| `r.curvenumber` | raster/curvenumber | Generates curve number raster from landcover |
| `r.damflood` | raster/damflood | The definition of flooding areas is of considerable |
| `r.denoise` | raster/denoise | Algorithm](https://www |
| `r.divergence` | raster/divergence | Magnitude and direction raster maps |
| `r.diversity` | raster/diversity | Calculates selected diversity indices by calling various |
| `r.droka` | raster/droka | The intersection between the topographic profile and the line starting |
| `r.earthworks` | raster/earthworks | Models new landforms |
| `r.edm.eval` | raster/edm | Addon returns a few evaluation statistics, including |
| `r.estimap.recreation` | raster/estimap | Implements the ESTIMAP recreation algorithm for ecosystem services mapping (Zulian 2014) |
| `r.euro.ecosystem` | raster/euro | Defines colors and raster category labels |
| `r.exdet` | raster/exdet | Correlative species distribution models (SDMs) often involve some degree |
| `r.extract` | raster/extract | Extracts selected categories from an integer raster map |
| `r.fidimo` | raster/fidimo | Performs analysis of fish dispersal based on leptokurtic |
| `r.fill.category` | raster/hydrologic_modeling | Replaces the values of pixels of a given category |
| `r.findtheriver` | raster/findtheriver | Finds the stream pixel nearest a given coordinate for watershed delineation |
| `r.flexure` | raster/flexure | Computes how the rigid outer shell of a planet deforms |
| `r.flip` | raster/flip | Flips a raster map in the North-South direction |
| `r.flowaccumulation` | raster/flowaccumulation | Calculates flow accumulation from a flow direction |
| `r.flowfill` | raster/flowfill | Runs the FlowFill code (Callaghan and Wickert, 2019) |
| `r.forcircular` | raster/forcircular | Analyzes and measures the level of sustainability |
| `r.forestfrag` | raster/forestfrag | Computes the forest fragmentation following |
| `r.fusion` | raster/image-fusion | Enhances the resolution of a raster map by using spatial |
| `r.futures` | raster/futures | FUTure Urban-Regional Environment Simulation (FUTURES) |
| `r.fuzzy.logic` | raster/fuzzy-logic | Performs fuzzy logical operations (AND, OR, NOT) on membership rasters from r.fuzzy |
| `r.fuzzy.set` | raster/fuzzy-logic | Name of input raster map to be fuzzified |
| `r.fuzzy.system` | raster/fuzzy-logic | A text file containing maps name and fuzzy sets connected with map |
| `r.gdd` | raster/gdd | Calculates (accumulated) growing degree days |
| `r.gradient` | raster/gradient | Create a gradient map. It is able to create horizontal |
| `r.gravity.terrain` | raster/gravity | The purpose of r |
| `r.green` | raster/green | Suite computes the residual energy potential of different renewable energies |
| `r.gsflow.hydrodem` | raster/gsflow | Generates a hydrologically-correct MODFLOW DEM |
| `r.gwr` | raster/gwr | Calculates a geographically weighted multiple linear regression |
| `r.hand` | raster/hand | Height Above Nearest Drainage (HAND) is a terrain-based model |
| `r.hants` | raster/hants | Performs a Harmonic ANalysis of Time Series (HANTS) analysis |
| `r.hazard.flood` | raster/hazard | Checks if an implementation of a fast procedure to detect |
| `r.houghtransform` | raster/houghtransform | Performing vector line extraction from edges |
| `r.hydro.flatten` | raster/hydrology | The tool derives single elevation value for water bodies based on lidar |
| `r.hydrobasin` | raster/hydrobasin | Delineates a large number of watersheds from a flow |
| `r.hydrodem` | raster/hydrodem | Applies hydrological conditioning (sink removal) |
| `r.hypso` | raster/hypso | Produces the hypsometric and hypsographic curve related |
| `r.in.ahn` | file/import_raster_data | Imports elevation data from the Actueel Hoogtebestand Nederland (AHN) |
| `r.in.nasadem` | file/import_raster_data | Imports all NASADEM tiles covering the current region or |
| `r.in.ogc` | file/import_raster_data | Suite is able to import raster data from several OGC API standards. It supports OGC API Coverages - import raster data |
| `r.in.srtm.region` | file/import_raster_data | Imports all SRTM tiles covering the current region or |
| `r.in.usgs` | file/import_raster_data | Downloads and patches selected USGS datasets (NED, NAIP |
| `r.in.vect` | file/import_raster_data | Transforms an external vector file (like GeoPackage) into a raster |
| `r.in.wcs` | file/import_raster_data | Downloads and imports coverage from WCS server |
| `r.jpdf` | raster/jpdf | Reads two series of raster maps and calculates for each raster |
| `r.lake.series` | raster/hydrologic_modeling | Fills a lake or any area from a given start point or |
| `r.landscape.evol` | raster/landscape-ecology | Takes as input a raster digital elevation model (DEM) |
| `r.landscape.evol.old` | raster/landscape-ecology | R.landscape.evol takes as input a raster digital elevation model (DEM) |
| `r.learn.ml` | raster/machine-learning | Represents a front-end to the scikit learn python package |
| `r.learn.ml2` | after:r.learn.ml | Applies scikit-learn classification and regression models to GRASS rasters (v2) |
| `r.lfp` | raster/lfp | Calculates the longest flow paths for given outlet points |
| `r.local.relief` | raster/local | Generates a local relief model (LRM) from lidar-derived |
| `r.manning` | raster/manning | Converts a land cover raster to a Manning's roughness |
| `r.mapcalc.tiled` | raster/raster_map_calculator | Cuts a raster input map into tiles and runs |
| `r.massmov` | raster/massmov | Checks if a numerical model that allows users to simulate |
| `r.maxent.lambdas` | raster/maxent-modelling | The script is intended to compute (raw) or (logistic) prediction |
| `r.maxent.predict` | raster/maxent-modelling | Module is a front-end to the Maxent software |
| `r.maxent.setup` | raster/maxent-modelling | Addon provides a helper function to install Maxent |
| `r.maxent.train` | raster/maxent-modelling | Module is a front-end to the Maxent software |
| `r.mblend` | raster/mblend | Implements Mblend, a DEM merging method proposed by Leitão |
| `r.mcda.ahp` | raster/multi-criteria-decision | Generate a raster map classified with analytic |
| `r.mcda.electre` | raster/multi-criteria-decision | Checks if the implementation of the ELECTRE multicriteria |
| `r.mcda.input` | raster/multi-criteria-decision | Module will read a \rls file output from jMAF, JAMM |
| `r.mcda.output` | raster/multi-criteria-decision | Module will export raster maps as a \isf file |
| `r.mcda.promethee` | raster/multi-criteria-decision | Checks if the implementation of the a multicriteria decision |
| `r.mcda.roughset` | raster/multi-criteria-decision | Checks if the python implementation of the dominance rough |
| `r.mcda.topsis` | raster/multi-criteria-decision | Implements the ideal point algorithms based on TOPSIS |
| `r.meb` | raster/meb | The Multivariate Environmental Bias (MEB) takes the medium conditions |
| `r.mess` | raster/mess | Computes multivariate environmental similarity to compare an area with a reference region |
| `r.mregression.series` | raster/mregression | To calculate multiple linear |
| `r.mwprecip` | raster/mwprecip | Estimates rainfall from commercial microwave link attenuation via PostgreSQL data |
| `r.neighborhoodmatrix` | raster/neighborhoodmatrix | Identifies all adjacency relations between raster objects (segments or clumps) |
| `r.niche.similarity` | raster/niche | Overlap between all pairs of input raster layers |
| `r.northerness.easterness` | raster/northerness | Calculates northerness, easterness |
| `r.null.all` | raster/develop_raster_map | Manages NULL (no-data) values in all raster maps |
| `r.object.activelearning` | raster/object-based-analysis | Uses SVM from the scikit-learn python package to perform |
| `r.object.spatialautocor` | raster/object-based-analysis | Calculates global spatial autocorrelation |
| `r.object.thickness` | raster/object-based-analysis | Evaluates the minimum, maximum, and mean thickness |
| `r.out.kde` | file/export_raster_map | Creates an image file (e.g., PNG, JPG, or GIF) |
| `r.out.legend` | file/export_raster_map | Script provides a quick way to create a smoothed |
| `r.out.maxent_swd` | file/export_raster_map | The script is intended to produce a set of SWD files as input to MaxEnt |
| `r.out.ntv2` | file/export_raster_map | Converts a pair of GRASS raster maps containing longitude |
| `r.patch.smooth` | raster/overlay_rasters | Module fuses rasters representing elevation together by patching them |
| `r.pi` | raster/pi | Description: Toolset for multiscale analysis of landscape patch structure |
| `r.popgrowth` | raster/popgrowth | R.popgrow calculates classical models of population growths |
| `r.pops.spread` | raster/pops-disease-modelling | All issues and imperfections of the code should be documented in this |
| `r.prominence` | raster/prominence | Calculates the average difference between a central cell |
| `r.quantile.ref` | raster/reports_and_statistics | Computes for each pixel the quantile ranking |
| `r.random.walk` | raster/generate_random_cells | Module generates a 2D random walk across the current |
| `r.random.weight` | raster/generate_random_cells | R.rand.weight generates a binary raster layer with a random selection |
| `r.recode.attr` | raster/change_category_values_and_labels | Reclass/recode a raster layer based on values in a CSV attribute table |
| `r.regression.series` | raster/reports_and_statistics | To calculate linear regression |
| `r.resamp.tps` | raster/develop_raster_map | Performs multivariate thin plate spline interpolation |
| `r.richdem.breachdepressions` | raster/richdem-terrain | Richard Barnes (algorithm and codebase), Andrew D |
| `r.richdem.filldepressions` | raster/richdem-terrain | Richard Barnes (algorithm and codebase), Andrew D |
| `r.richdem.flowaccumulation` | raster/richdem-terrain | Richard Barnes (algorithm and codebase), Andrew D |
| `r.richdem.resolveflats` | raster/richdem-terrain | Richard Barnes (algorithm and codebase), Andrew D |
| `r.richdem.terrainattribute` | raster/richdem-terrain | Richard Barnes (algorithm and codebase), Andrew D |
| `r.rock.stability` | raster/rock | Two important steps can be recognised in the rockfall analysis |
| `r.roughness.vector` | raster/roughness | In this script surface roughness is taken as the dispersion of vectors |
| `r.runoff` | raster/runoff | Computes event-based runoff using the SCS Curve Number (SCS-CN) |
| `r.sample.category` | raster/sample | Generates points at random locations. Each category |
| `r.scatterplot` | raster/scatterplot | Creates a scatter plot of raster maps |
| `r.seasons` | raster/seasons | Counts the number of seasons in a time series. A season is |
| `r.series.boxplot` | raster/overlay_rasters | R.seriesboxplot draws boxplots of series of input raster layers. It |
| `r.series.decompose` | raster/overlay_rasters | To calculate decomposintion of signal |
| `r.series.diversity` | raster/overlay_rasters | Computes one or more diversity indices based on 2 |
| `r.series.filter` | raster/overlay_rasters | To filter raster time series X in time |
| `r.series.lwr` | raster/overlay_rasters | Local weighted regression (LWR) of raster time series for gap-filling and outlier removal |
| `r.shaded.pca` | raster/shaded | For the generation of RGB composite |
| `r.shalstab` | raster/shalstab | Allows to apply the algorithm developed by Montgomery |
| `r.sim.terrain` | raster/hydrologic_modeling | Checks if a short-term landscape evolution model that simulates |
| `r.sim.water.mp` | raster/hydrologic_modeling | Checks if a landscape scale simulation model of overland flow |
| `r.skyline` | raster/skyline | Takes a viewshed map as input and analyses the horizon. It |
| `r.skyview` | raster/skyview | Computes sky-view factor, a relief visualization technique (Zaksek et al. 2011) |
| `r.slope.direction` | raster/terrain_analysis | Computes slope as elevation difference divided |
| `r.slopeunits` | raster/slopeunits | Checks if a GRASS GIS addon toolset that creates, cleans |
| `r.smooth.seg` | imagery/filter_image | Generates a piece-wise smooth approximation |
| `r.soillossbare` | raster/soillossbare | Calculates annual soil loss in t/(ha·a) for bare soil using the USLE/RUSLE approach |
| `r.soils.texture` | raster/soils | Define soils texture from sand and clay distribution |
| `r.stone` | raster/stone | Three-dimensional modeling of rockfall trajectories |
| `r.stream.basins` | raster/hydrologic_modeling | Delineates basins according stream network |
| `r.stream.channel` | raster/hydrologic_modeling | Properties of the stream network |
| `r.stream.distance` | raster/hydrologic_modeling | Calculates distance to and elevation above streams and outlet |
| `r.stream.order` | raster/hydrologic_modeling | Calculates Strahler's and more streams hierarchy |
| `r.stream.segment` | raster/hydrologic_modeling | The main idea comes from works of Horton (1932) and Howard (1971, 1990) |
| `r.stream.slope` | raster/hydrologic_modeling | Calculates local downstream elevation change and local |
| `r.stream.snap` | raster/hydrologic_modeling | Snap point to modelled stream network |
| `r.stream.stats` | raster/hydrologic_modeling | Drainage network Stats |
| `r.stream.variables` | raster/hydrologic_modeling | Calculation of contiguous stream-specific variables that account |
| `r.stream.watersheds` | raster/hydrologic_modeling | Delineate the upstream contributing area ('sub-watershed') and stream |
| `r.subdayprecip.design` | raster/subdayprecip | Computes subday design precipitation totals |
| `r.suitability.regions` | raster/suitability | Converts a suitability raster into delineated suitable regions |
| `r.sun.daily` | raster/solar_radiance_and_shadows | Script for running r.sun for multiple days |
| `r.sun.hourly` | raster/solar_radiance_and_shadows | Script for running r.sun for multiple |
| `r.sun.mp` | raster/solar_radiance_and_shadows | Solar irradiance and irradiation model |
| `r.surf.idw2` | after:r.surf.idw | Fills a raster with IDW-interpolated values using an enhanced algorithm |
| `r.surf.nnbathy` | raster/generate_surfaces | Interface between the external nnbathy utility |
| `r.survey` | raster/survey | Earth surface analysis. |
| `r.terrain.texture` | raster/terrain-analysis | Calculates the nested-means terrain classification |
| `r.texture.tiled` | raster/terrain_analysis | Cuts a raster input map into tiles and runs |
| `r.threshold` | raster/threshold | Finds optimal threshold for stream extraction |
| `r.timeofconcentration` | raster/timeofconcentration | Generates a raster map of time |
| `r.to.vect.lines` | file/map_type_conversions | Extracts rows from a raster map as 3D vector lines |
| `r.to.vect.tiled` | file/map_type_conversions | Vectorizes the input raster map and produces |
| `r.tpi` | raster/tpi | Calculates a multiscale version of the Topographic Position Index |
| `r.traveltime` | raster/traveltime | Estimation of travel times/isochrones |
| `r.tri` | raster/tri | Calculates the Terrain Ruggedness Index (TRI) |
| `r.univar2` | after:r.univar | Computes univariate statistics for one or several raster maps (extended r.univar) |
| `r.valley.bottom` | raster/valley | Calculates the Multi-resolution Valley Bottom Flatness |
| `r.vect.stats` | raster/vect | Bins points from a vector map into a raster map |
| `r.vector.ruggedness` | raster/vector | Represents a measurement of terrain ruggedness |
| `r.viewshed.cva` | raster/terrain_analysis | Constructs cumulative viewshed (visualscape) maps from a set of viewpoints |
| `r.viewshed.exposure` | raster/terrain_analysis | Computes visual exposure to given exposure |
| `r.vif` | raster/vif | Module computes the variance inflation factor |
| `r.vol.dem` | raster/vol | Interpolates a voxel model from a series of DEMs by flood |
| `r.wateroutlet.lessmem` | raster/wateroutlet | Memory-efficient watershed basin delineation from drainage direction map |
| `r.width.funct` | raster/width | Produces the Width Function of a basin. |
| `r.windfetch` | raster/windfetch | Computes wind fetch — open-water distance over which wind blows unobstructed |
| `r.zonal.classes` | raster/zonal-statistics | Computes class proportions and majority class (mode) |
| `r3.count.categories` | 3d_raster/count | Iterates over whole numbers (categories, classes) |
| `r3.forestfrag` | 3d_raster/forestfrag | Computes 3D forest fragmentation index using sliding-window algorithm (Riitters 2000) |
| `r3.profile` | 3d_raster/profile | Program outputs two or four column (with -g) data to stdout or |
| `r3.scatterplot` | 3d_raster/scatterplot | Creates a scatter plot of 3D raster maps |
| `r3.to.group` | file/map_type_conversions | Converts a 3D raster map to imagery group |
| `r3.to.mem` | file/map_type_conversions | Extracts one or more 2D raster slices from a 3D raster (voxel) map and places them in a RAM-backed in-memory GRASS |
| `r3.what` | 3d_raster/what | Queries 3D raster at specified coordinates |
| `t.rast.boxplot` | temporal/manage_maps_in_datasets | Draws boxplots of the raster in a space-time raster |
| `t.rast.import.netcdf` | temporal/manage_maps_in_datasets | Imports content of one or more NetCDF files |
| `t.rast.kappa` | temporal/manage_maps_in_datasets | Calculate kappa parameter in a space time raster |
| `t.rast.line` | temporal/manage_maps_in_datasets | Draws trend lines of the average values of the input |
| `t.rast.null` | temporal/manage_maps_in_datasets | Manages NULL-values of the input space time raster |
| `t.rast.out.xyz` | temporal/manage_maps_in_datasets | Module exports a space time raster dataset as a list |
| `t.rast.patch` | temporal/manage_maps_in_datasets | Patches raster maps that have gaps in time with subsequent |
| `t.rast.what.aggr` | temporal/manage_maps_in_datasets | Samples a space time raster dataset at points |
| `t.rast.whatcsv` | temporal/manage_maps_in_datasets | Samples a space time raster dataset |
| `t.stac` | temporal/stac | Toolset for working with SpatioTemporal Asset Catalogs |
| `v.area.stats` | vector/area-analysis | Populates attribute values from vector features |
| `v.area.weigh` | vector/area-analysis | Rasterizes vector areas using known cell weights |
| `v.boxplot` | vector/boxplot | Module draws a boxplot of the values in a vector map |
| `v.build.pg` | vector/topology_maintenance | Builds PostGIS topology |
| `v.centerline` | vector/centerline | Creates a new map with a line representing its center |
| `v.centerpoint` | vector/centerpoint | Calculates center points for point clouds, lines |
| `v.civil` | vector/civil | Generates alignment for road/channel/port design with horizontal polygon input |
| `v.class.ml` | vector/reports_and_statistics | Uses machine-learning algorithms to classify a vector |
| `v.class.mlR` | vector/reports_and_statistics | Uses the R caret package for vector classification |
| `v.class.mlpy` | vector/reports_and_statistics | Supervised vector classification |
| `v.clean.ogr` | vector/topology_maintenance | Cleans non-topological polygons in an OGR datasource |
| `v.colors2` | after:v.colors | Assigns and manages color tables for vector maps (extended v.colors) |
| `v.concave.hull` | vector/concave | Creates a concave hull around points. Contrary |
| `v.convert` | vector/format-conversion | Converts GRASS 5.0/5.3/5.4 vectors into 5.7/6.x vectors |
| `v.convert.all` | vector/format-conversion | Converts all GRASS 5.0/5.3/5.4 vectors in the current |
| `v.db.pyupdate` | vector/query_vector_map | Assigns a new value to a column in the attribute table |
| `v.delaunay3d` | vector/delaunay3d | Performs 3D Delaunay triangulation on input 3D vector |
| `v.ellipse` | vector/ellipse | Computes the best-fitting ellipse for input vector map |
| `v.explode` | vector/explode | "explode" polylines, splitting them to separate lines |
| `v.external.all` | file/link_external_data | Links all OGR layers from a datasource as pseudo-vector maps in current mapset |
| `v.faultdirections` | vector/faultdirections | Draws a polar barplot of fault directions |
| `v.feature.algebra` | vector/feature | Vector calculator |
| `v.fixed.segmentpoints` | vector/fixed | Creates segment points along a vector line |
| `v.flexure` | vector/flexure | Computes how the rigid outer shell of a planet deforms |
| `v.greedycolors` | vector/greedycolors | Assigns numbers to areas that are not adjacent |
| `v.gsflow.export` | vector/gsflow | Produces output tables and maps that are used |
| `v.gsflow.gravres` | vector/gsflow | Produces "gravity reservoirs" that are used |
| `v.gsflow.grid` | vector/gsflow | Generates the MODFLOW grid and associated basin mask |
| `v.gsflow.hruparams` | vector/gsflow | V.gsflow.hruparameters uploads required HRU parameters |
| `v.gsflow.mapdata` | vector/gsflow | Add attributes from any raster |
| `v.gsflow.reaches` | vector/gsflow | Produces channel "reaches" |
| `v.gsflow.segments` | vector/gsflow | Produces channel segments |
| `v.habitat.dem` | vector/habitat-analysis | Calculates DEM and solar derived characteristics |
| `v.histogram` | vector/histogram | Draws a histogram of the values in a vector map attribute |
| `v.in.csv` | file/import_vector_data | Imports rows from a CSV (Comma Separated Value) file |
| `v.in.gbif` | file/import_vector_data | Imports GBIF species distribution |
| `v.in.geopaparazzi` | file/import_vector_data | Imports all elements saved into Geopaparazzi |
| `v.in.gns` | file/import_vector_data | Imports US-NGA GEOnet Names Server (GNS) country files |
| `v.in.gps` | file/import_vector_data | Allows the user to import waypoint, route, and track data |
| `v.in.natura2000` | file/import_vector_data | Imports Natura 2000 data |
| `v.in.ogc` | file/import_vector_data | Import vector data from OGC API standards |
| `v.in.osm` | file/import_vector_data | Imports OpenStreetMap data |
| `v.in.ply` | file/import_vector_data | Imports a vector map from a PLY (Polygon File Format / Stanford format) file |
| `v.in.pygbif` | file/import_vector_data | Downloads species occurrence data from GBIF using pygbif search and filter criteria |
| `v.in.redlist` | file/import_vector_data | Imports IUCN Red List spatial data (species range maps) as vector |
| `v.in.redwg` | file/import_vector_data | Imports DWG files into GRASS |
| `v.in.survey` | file/import_vector_data | Imports points from ASCII file |
| `v.in.wfs2` | after:v.in.wfs | Imports OGC WFS map features using an enhanced WFS client (v2) |
| `v.isochrones` | vector/isochrones | Creates a vector map of isochrones |
| `v.krige` | vector/krige | Performs Kriging operations |
| `v.kriging` | vector/kriging | Constructs 2D / 3D raster interpolation |
| `v.label.sa` | vector/develop_vector_map | Makes a label-file from a GRASS vector map with labels |
| `v.lidar.mcc` | vector/lidar_analysis | Modified implementation of the Multiscale Curvature |
| `v.link.precip` | vector/link | Links time-windowed precipitation data to a vector link map |
| `v.mapcalc` | vector/mapcalc | Performs overlay and buffer functions on vector map layers |
| `v.maxent.swd` | vector/maxent-modelling | Takes one or more point vector layers |
| `v.median` | vector/median | Create the median point of a cloud of point |
| `v.mrmr` | vector/mrmr | Checks if a simple GUI for exporting data to the Minimum |
| `v.multi2singlepart` | vector/multi2singlepart | Creates a vector layer containing singlepart |
| `v.neighborhoodmatrix` | vector/neighborhoodmatrix | Identifies all adjacency relations between |
| `v.net.curvedarcs` | vector/network_analysis | Draws curved arcs between origin-destination points (e.g. flow volumes) |
| `v.net.salesman.opt` | vector/network_analysis | Creates a cycle connecting given nodes (Traveling salesman problem) |
| `v.nnstat` | vector/nnstat | Indicates clusters, separations or random distribution |
| `v.out.gps` | file/export_vector_map | Allows the user to export waypoint, route, and track data |
| `v.out.ply` | file/export_vector_map | Converts a GRASS vector map in binary format to an ASCII |
| `v.out.png` | file/export_vector_map | Exports a GRASS vector map in non-georeferenced Portable |
| `v.percolate` | vector/percolate | Implements continuum percolation analysis. It identifies |
| `v.ply.rectify` | vector/ply | Imports a PLY point cloud, georeferences and exports it |
| `v.profile.points` | vector/reports_and_statistics | Creates profile (transect) from vector point map or |
| `v.rast.bufferstats` | vector/update_attributes | The purpose of the v |
| `v.rast.move` | vector/update_attributes | Takes values from raster maps and adds them to X and Y |
| `v.scatterplot` | vector/scatterplot | Draws a scatterplot of the value in one column against |
| `v.sort.points` | vector/sort | Takes an input point vector map, sorts the points |
| `v.stats` | vector/stats | Calculates vector statistics |
| `v.strds.stats` | vector/strds | Calculates univariate statistics from given space-time |
| `v.stream.inbasin` | vector/stream-networks | Uses the output of v.stream.network to select only |
| `v.stream.network` | vector/stream-networks | Populates the attribute table of a river network |
| `v.stream.order` | vector/stream-networks | Order approaches based on a stream network vector map and a water outlet |
| `v.stream.profiler` | vector/stream-networks | Builds and exports plots and data sets of river long |
| `v.surf.icw` | raster/interpolate_surfaces | Inverse cost weighting is like inverse distance weighted (IDW) |
| `v.surf.mass` | raster/interpolate_surfaces | Creates a raster surface from vector areas, preserving |
| `v.surf.nnbathy` | raster/interpolate_surfaces | Checks if an interface between the external nnbathy utility |
| `v.surf.rst.cv` | raster/interpolate_surfaces | Cross-validation of regularized spline with tension (RST) |
| `v.surf.tps` | raster/interpolate_surfaces | Performs multivariate thin plate spline interpolation |
| `v.tin.to.rast` | vector/tin | Converts (rasterizes) a TIN map into a raster map |
| `v.to.rast.multi` | file/map_type_conversions | Creates a raster map for each selected numeric column |
| `v.transects` | vector/transects | Creates equally spaced geometries along input lines |
| `v.vect.stats.multi` | vector/update_attributes | Computes attribute statistics of points in vector |
| `v.vol.idw` | 3d_raster | Fills a RASTER3D raster volume matrix with interpolated |
| `v.what.rast.label` | vector/query_vector_map | Retrieves raster values and labels from one or more |
| `v.what.rast.multi` | vector/query_vector_map | Retrieves raster value from a given set of raster |
| `v.what.spoly` | vector/query_vector_map | Queries vector map with overlaping "spaghetti" polygons |
| `v.what.strds.timestamp` | vector/query_vector_map | Matches points with a timestamp |
| `p.in.pds3` | planetary/import_export | Import PDS3 planetary image (IMAGE/QUBE/SPECTRAL_QUBE) into GRASS raster map(s) |
| `p.in.pds4` | planetary/import_export | Import PDS4 planetary data product (Array_2D/3D_Image) into GRASS raster |
| `p.in.isis` | planetary/import_export | Import ISIS3 cube (.cub) into GRASS raster via GDAL ISIS3 driver |
| `p.out.isis` | planetary/import_export | Export GRASS raster to ISIS3 cube with PVL metadata |
| `p.spiceinit` | planetary/camera_and_geometry | Attach NAIF SPICE kernel paths to a GRASS raster map |
| `p.caminfo` | planetary/camera_and_geometry | Report camera geometry (resolution, coverage, radius) for a planetary raster |
| `p.cam2map` | planetary/camera_and_geometry | Project a raw planetary image to map coordinates using ellipsoid back-projection |
| `p.phocube` | planetary/photometric_analysis | Generate incidence/emission/phase/lat/lon/radius backplane rasters |
| `p.photomet` | planetary/photometric_analysis | Apply photometric correction (Hapke/Minnaert/Lambert/LunarLambert) |
| `p.atcorr.hapke` | planetary/photometric_analysis | Hapke-model atmospheric correction (Isotropic/Anisotropic scattering) |
| `p.atcorr` | planetary/photometric_analysis | Body-aware atmospheric correction: per-pixel retrieval (none/thin/thick regimes) |
| `p.photrim` | planetary/photometric_analysis | Trim raster by photometric angle thresholds |
| `p.albedo` | planetary/photometric_analysis | Compute geometric or Bond albedo from photometric model |
| `p.spectral.planet` | planetary/spectral_mineral | Spectral high-pass, divisive filter, band ratio, band depth for planetary cubes |
| `p.mineral.indices` | planetary/spectral_mineral | Planetary mineral spectral indices (olivine, pyroxene, TiO2, FeO, mafic) |
| `p.specpix` | planetary/spectral_mineral | Classify pixels into ISIS3 special pixel categories (NULL/LRS/LIS/HIS/HRS) |
| `p.dem.prep` | planetary/terrain_analysis | Prepare planetary DEM: void fill, radius-to-height conversion, outlier clip |
| `p.slope.planet` | planetary/terrain_analysis | Slope and aspect with spherical arc-length corrections for non-Earth bodies |
| `p.shadow.planet` | planetary/terrain_analysis | Ray-marching shadow mask for planetary surfaces |
| `p.rings.project` | planetary/ring_plane | Project raster to/from ring-plane cylindrical coordinates |
| `p.rings.stats` | planetary/ring_plane | Azimuthal and radial statistics for ring-plane rasters |
| `p.dstripe` | planetary/noise_calibration | Remove detector-column stripe noise from pushbroom planetary images |
| `p.desmear` | planetary/noise_calibration | Correct frame-transfer readout smear in planetary framing cameras |
| `p.cubenorm` | planetary/noise_calibration | Column/row normalisation for planetary pushbroom images |
| `p.bandnorm` | planetary/noise_calibration | Band-by-band normalisation for multi-spectral planetary cubes |
| `p.automos` | planetary/mosaic | Build a seamless planetary mosaic from multiple rasters with optional equalisation |
| `p.crater.freq` | planetary/utilities | Crater size-frequency distribution and surface age estimation (Neukum/Hartmann) |
| `p.target.info` | planetary/utilities | Physical constants for solar system bodies (radii, GM, rotation, frame) |
| `r.in.sentinel` | raster/import  ⚠️ NEW — review placement |  |
| `r.in.ssurgo` | raster/import  ⚠️ NEW — review placement | DESCRIPTION r |
| `r.noaa.atlas14` | raster/noaa  ⚠️ NEW — review placement | DESCRIPTION r |
| `r.richdem` | raster/richdem-terrain  ⚠️ NEW — review placement | DESCRIPTION r |
| `r.upflowlength` | raster/upflowlength  ⚠️ NEW — review placement | Calculates upstream flow length from a flow direction raster map |
| `d.osint` | settings/osint  ⚠️ NEW — review placement | DESCRIPTION d |
| `d.osm` | settings/osm  ⚠️ NEW — review placement | DESCRIPTION d |
| `i.hyper.endmembers` | imagery/hyperspectral-analysis  ⚠️ NEW — review placement | - GRASS GIS manual DESCRIPTION i |
| `i.hyper.lib_ecosis` | imagery/hyperspectral-analysis  ⚠️ NEW — review placement | - GRASS GIS manual DESCRIPTION i |
| `i.hyper.lib_gsti` | imagery/hyperspectral-analysis  ⚠️ NEW — review placement | - GRASS GIS manual DESCRIPTION i |
| `i.hyper.lib_relab` | imagery/hyperspectral-analysis  ⚠️ NEW — review placement | - GRASS GIS manual DESCRIPTION i |
| `i.hyper.lib_usgs` | imagery/hyperspectral-analysis  ⚠️ NEW — review placement | - GRASS GIS manual DESCRIPTION i |
| `i.hyper.speclookup` | imagery/hyperspectral-analysis  ⚠️ NEW — review placement | - GRASS GIS manual DESCRIPTION i |
| `i.omnicloudmask` | imagery/omnicloudmask  ⚠️ NEW — review placement | DESCRIPTION The i |
| `r.hydro.hbv` | raster/hydrology  ⚠️ NEW — review placement | HBV lumped-basin rainfall-runoff model with Monte-Carlo/GLUE |
| `r.hydro.hbv.basins` | raster/hydrology  ⚠️ NEW — review placement | DESCRIPTION r |
| `r.hydro.hbv.forcing` | raster/hydrology  ⚠️ NEW — review placement | DESCRIPTION r |
| `r.in.emit` | raster/import  ⚠️ NEW — review placement | DESCRIPTION r |
| `r.in.geoglam` | raster/import  ⚠️ NEW — review placement | DESCRIPTION r |
| `r.in.landsat` | raster/import  ⚠️ NEW — review placement | DESCRIPTION r |
| `t.crop.ef` | temporal/crop  ⚠️ NEW — review placement | DESCRIPTION t |
| `t.crop.season` | temporal/crop  ⚠️ NEW — review placement | DESCRIPTION A GRASS GIS addon that derives crop development stage and irrigation status from fused Sentinel-1 SAR and Se |
| `t.crop.yield` | temporal/crop  ⚠️ NEW — review placement | DESCRIPTION t |
| `t.in.era5` | temporal/import  ⚠️ NEW — review placement | DESCRIPTION t |
| `t.rast.stl` | temporal/raster-time-series  ⚠️ NEW — review placement | DESCRIPTION t |
| `v.in.ags` | vector/import  ⚠️ NEW — review placement | DESCRIPTION v |
| `v.in.gazetteer` | vector/import  ⚠️ NEW — review placement | DESCRIPTION v |
| `v.in.warspotting` | vector/import  ⚠️ NEW — review placement | DESCRIPTION v |
| `v.tonlesap.model` | vector/tonlesap  ⚠️ NEW — review placement | Multi-criteria pairwise-comparison outranking model for site suitability (Tonle Sap villages) |
