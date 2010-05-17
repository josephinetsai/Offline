//
// Construct the Mu2e G4 world and serve information about that world.
//
// $Id: Mu2eWorld.cc,v 1.24 2010/05/17 21:47:33 genser Exp $
// $Author: genser $ 
// $Date: 2010/05/17 21:47:33 $
//
// Original author Rob Kutschke
//
//  Heirarchy is:
//  0      World (air)
//  1      Earthen Overburden
//  2      Concrete walls of the hall
//  3      Air inside the hall
//  4      Concrete shielding around the DS.
//  5      Air inside the concrete sheilding
//  6      Iron cosmic ray absorber.
//  7      Air inside the cosmic ray absorber
//  8      Effective volume representing the DS coils+cryostats.
//  9      Vacuum inside of the DS coils.
//
//  4      Effective volume representing the PS coils+cryostats.
//  5      Vacuum inside of the PS coils.
//
//  The Earth overburden is modelling in two parts: a box that extends
//  to the surface of the earth plus a cap above grade.  The cap is shaped
//  as a G4Paraboloid.
//
// Notes:
// 1) G4VisAttributes is screwed up in this routine and everywhere.
//    The logical volume class never takes ownership of the G4VisAttibutes.
//    It simply leaks them.  We can pass vis attributes by const ref and
//    G4 will make a copy that it subsequently leaks at destructor time.
//    We can pass it in by pointer; then it is our job to keep it alive for the
//    full lifetime of the logical volume and then to delete it after that.
//

// C++ includes
#include <iostream>

// Framework includes
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/Exception.h"

// Mu2e includes
#include "Mu2eG4/inc/Mu2eWorld.hh"
#include "Mu2eG4/inc/MaterialFinder.hh"
#include "Mu2eG4/inc/StrawPlacer.hh"
#include "Mu2eG4/inc/StrawSD.hh"
#include "Mu2eG4/inc/findMaterialOrThrow.hh"
#include "Mu2eG4/inc/nestBox.hh"
#include "Mu2eG4/inc/nestTubs.hh"
#include "Mu2eG4/inc/nestTorus.hh"
#include "Mu2eG4/inc/ITrackerBuilder.hh"
#include "GeometryService/inc/GeometryService.hh"
#include "GeometryService/inc/GeomHandle.hh"
#include "TargetGeom/inc/Target.hh"
#include "Mu2eG4/inc/constructLTracker.hh"
#include "Mu2eG4/inc/constructTTracker.hh"
#include "Mu2eG4/inc/constructDummyTracker.hh"
#include "Mu2eG4/inc/constructStoppingTarget.hh"
#include "Mu2eG4/inc/constructDummyStoppingTarget.hh"
#include "Mu2eG4/inc/constructCalorimeter.hh"

// G4 includes
#include "G4GeometryManager.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4SolidStore.hh"
#include "G4Material.hh"
#include "G4Box.hh"
#include "G4Paraboloid.hh"
#include "G4Colour.hh"
#include "G4Tubs.hh"
#include "G4LogicalVolume.hh"
#include "G4ThreeVector.hh"
#include "G4PVPlacement.hh"
#include "globals.hh"
#include "G4VisAttributes.hh"
#include "G4UniformMagField.hh"
#include "G4FieldManager.hh"
#include "G4Mag_UsualEqRhs.hh"
#include "G4ExactHelixStepper.hh"
#include "G4ChordFinder.hh"
#include "G4TransportationManager.hh"
#include "G4UserLimits.hh"
#include "G4SDManager.hh"
#include "G4ClassicalRK4.hh"
#include "G4CashKarpRKF45.hh"
#include "G4ImplicitEuler.hh"
#include "G4ExplicitEuler.hh"

//Julie's DSField for reading in the file for full DS field
#include "Mu2eG4/inc/DSField.hh"

using namespace std;

namespace mu2e {

  Mu2eWorld::Mu2eWorld()
    :  _cosmicReferencePoint(),
       _mu2eOrigin(),
       _info(),
       _detSolUpstreamBField(),
       _usualUpstreamRHS(),
       _exactUpstreamHelix(),
       _chordUpstreamFinder(),
       _fieldUpstreamMgr(),
       _stepUpstreamLimit(),
       _detSolDownstreamBField(),
       _usualDownstreamRHS(),
       _exactDownstreamHelix(),
       _chordDownstreamFinder(),
       _fieldDownstreamMgr(),
       _stepDownstreamLimit(){
  }
  
  Mu2eWorld::~Mu2eWorld(){

    // Do not destruct the solids, logical volumes or physical volumes.
    // G4 looks after that itself.

  }

  void printPhys() {
    G4PhysicalVolumeStore* pstore = G4PhysicalVolumeStore::GetInstance();
    int n(0);
    for ( std::vector<G4VPhysicalVolume*>::const_iterator i=pstore->begin(); i!=pstore->end(); i++){
      cout << "Physical Volume: "
           << setw(5) << n++
           << (*i)->GetName()
           << endl;
      if ( n > 25 ) break;
    }

  }


  // This is the callback used by G4.
  WorldInfo const* Mu2eWorld::construct(){
    
    // Get access to the master geometry system and its run time config.
    edm::Service<GeometryService> geom;
    SimpleConfig const& config = geom->config();
    _config = &config;
    
    // Construct a world with nothing in it.
    constructWorld(config);

    return &_info;
  }

  void Mu2eWorld::constructWorld( SimpleConfig const& config ){

    MaterialFinder materialFinder(config);

    // Dimensions and material of the world.
    vector<double> worldHLen;
    _config->getVectorDouble("world.halfLengths", worldHLen,3);
    setUnits ( worldHLen, CLHEP::mm );
    G4Material* worldMaterial = materialFinder.get("world.materialName");


    // A number of objects are referenced to the solenoids.
    double prodSolXoff = _config->getDouble("world.prodSolXoff") * CLHEP::mm;
    double detSolXoff  = -prodSolXoff;
    double dsz0        = _config->getDouble("toyDS.z0") * CLHEP::mm;

    // Half length of the detector solenoid.
    double dsHalfLength = _config->getDouble("toyDS.halfLength")* CLHEP::mm;
    
    // Construct the world volume.
    string worldName("World");
    _info.worldSolid = new G4Box( worldName, worldHLen[0], worldHLen[1], worldHLen[2] );
    _info.worldLog   = new G4LogicalVolume( _info.worldSolid, worldMaterial, worldName );
    _info.worldPhys  = new G4PVPlacement( 0, G4ThreeVector(), _info.worldLog, worldName, 
                                          0, 0, 0);
    _info.worldLog->SetVisAttributes(G4VisAttributes::Invisible);

    // Get parameters related to the overall dimensions of the hall and to
    // the earthen overburden.
    double floorThick           = CLHEP::mm * _config->getDouble("hall.floorThick");
    double ceilingThick         = CLHEP::mm * _config->getDouble("hall.ceilingThick");
    double wallThick            = CLHEP::mm * _config->getDouble("hall.wallThick");
    double overburdenDepth      = CLHEP::mm * _config->getDouble("dirt.overburdenDepth");
    vector<double> hallInHLen;
    _config->getVectorDouble("hall.insideHalfLengths",hallInHLen,3);
    setUnits( hallInHLen, CLHEP::mm);

    // Derived parameters.
    G4Material* dirtMaterial = materialFinder.get("dirt.overburdenMaterialName");

    // Top of the floor in G4 world coordinates.
    double yFloor   = -worldHLen[1] + floorThick;

    // The height above the floor of the y origin of the Mu2e coordinate system.
    double yOriginHeight = _config->getDouble("world.mu2eOrigin.height" )*CLHEP::mm;

    // Position of the origin of the mu2e coordinate system
    _mu2eOrigin = G4ThreeVector( 
                                _config->getDouble("world.mu2eOrigin.xoffset")*CLHEP::mm,
                                yOriginHeight + yFloor,
                                _config->getDouble("world.mu2eOrigin.zoffset")*CLHEP::mm
                                );
    edm::LogInfo log("GEOM");
    log << "Mu2e Origin: " << _mu2eOrigin << "\n";

    // Origin used to construct the MECO detector.
    _mu2eDetectorOrigin = _mu2eOrigin + G4ThreeVector( -3904., 0., 12000.);
    log << "Mu2e Detector Origin: " << _mu2eDetectorOrigin << "\n";

    // Bottom of the ceiling in G4 world coordinates.
    double yCeilingInSide = yFloor + 2.*hallInHLen[1];
    
    // Top of the ceiling in G4 world coordinates.
    double yCeilingOutside  = yCeilingInSide + ceilingThick;

    // Surface of the earth in G4 world coordinates.
    double ySurface  = yCeilingOutside + overburdenDepth;
    
    // Half length and y origin of the dirt box.
    double yLDirt = ( ySurface + worldHLen[1] )/2.;
    double y0Dirt = -worldHLen[1] + yLDirt;
    
    // Center of the dirt box, in the G4 world system.
    G4ThreeVector dirtOffset(0.,y0Dirt,0.);
    
    // Half lengths of the dirt box.
    double dirtHLen[3] = { worldHLen[0], yLDirt, worldHLen[2] };
    
    // Main body of dirt around the hall.
    VolumeInfo dirtInfo = nestBox( "DirtBody",
                                   dirtHLen,
                                   dirtMaterial,
                                   0,
                                   dirtOffset,
                                   _info.worldLog,
                                   0,
                                   G4Colour::Magenta()
                                   );
    
    // Dirt cap is modeled as a paraboloid.
    double capHalfHeight  = CLHEP::mm * _config->getDouble("dirt.capHalfHeight");
    double capBottomR     = CLHEP::mm * _config->getDouble("dirt.capBottomRadius");
    double capTopR        = CLHEP::mm * _config->getDouble("dirt.capTopRadius");

    // The top of the world.
    double yEverest = ySurface + 2.*capHalfHeight;

    // Selfconsistency check.
    if ( yEverest > 2.*worldHLen[1] ){
      throw cms::Exception("GEOM")
        << "Top of the world is outside of the world volume! \n";
    }

    // Build the reference points that others will use.
    _cosmicReferencePoint = G4ThreeVector( 0., yEverest, 0.);
    log << "Cosmic Ref: " << _cosmicReferencePoint << "\n";

    // Construct the cap.
    string dirtCapName("DirtCap");
    G4Paraboloid* dirtCapSolid = new
      G4Paraboloid( dirtCapName, capHalfHeight, capTopR, capBottomR);
    
    G4LogicalVolume* dirtCapLog = new
      G4LogicalVolume( dirtCapSolid, 
                       dirtMaterial, 
                       dirtCapName);
    
    G4RotationMatrix* dirtCapRot = new G4RotationMatrix();
    dirtCapRot->rotateX( -90*CLHEP::degree);
    
    G4VPhysicalVolume* dirtCapPhys =  new
      G4PVPlacement( dirtCapRot, 
                     G4ThreeVector( detSolXoff, ySurface+capHalfHeight, dsz0+_mu2eOrigin.z()), 
                     dirtCapLog, 
                     dirtCapName, 
                     _info.worldLog, 
                     0, 
                     0);
    
    G4VisAttributes* dirtCapVisAtt = new G4VisAttributes(true, G4Colour::Green());
    dirtCapVisAtt->SetForceSolid(true);
    dirtCapLog->SetVisAttributes(dirtCapVisAtt);

    // Position of the center of the hall in the world volume.
    vector<double> hallPosition;
    _config->getVectorDouble("hall.offset", hallPosition,3);
    setUnits( hallPosition, CLHEP::mm);
    double hallY0 = yFloor + hallInHLen[1] + hallPosition[1];

    // Materials for the hall walls and the interior of the hall
    G4Material* wallMaterial = materialFinder.get("hall.wallMaterialName");
    G4Material* hallMaterial = materialFinder.get("hall.insideMaterialName");

    // Half lengths of the exterior of the concrete for the hall walls.
    double hallOutHLen[3] ={
      hallInHLen[0] + wallThick,
      hallInHLen[1] + ( ceilingThick + floorThick )/2.,
      hallInHLen[2] + wallThick
    };
    
    // Center of the concrete volume in the coordinate system of the dirt.
    G4ThreeVector wallOffset = 
      G4ThreeVector(hallPosition[0], hallY0, hallPosition[2]) - dirtOffset;

    // Origin of the hall air volume in the system of the hall concrete volume.
    G4ThreeVector hallOffset( 0., (floorThick-ceilingThick)/2., 0.);

    // Concrete walls of the hall.
    VolumeInfo wallInfo = nestBox( "HallWalls",
                                   hallOutHLen,
                                   wallMaterial,
                                   0,
                                   wallOffset,
                                   dirtInfo.logical,
                                   0,
                                   G4Colour::Red()
                                   );
    
    // Air volume inside of the hall.
    VolumeInfo hallInfo = nestBox( "HallAir",
                                   hallInHLen,
                                   hallMaterial,
                                   0,
                                   hallOffset,
                                   wallInfo.logical,
                                   0,
                                   G4Colour::Red()
                                   );
    
    // Concrete shield.
    double shieldConXSpace           = CLHEP::mm * _config->getDouble("shieldCon.xspace");
    double shieldConInsideHeight     = CLHEP::mm * _config->getDouble("shieldCon.insideHeight");
    double shieldConInsideHalfLength = CLHEP::mm * _config->getDouble("shieldCon.insideHalfLength");
    double shieldConThick            = CLHEP::mm * _config->getDouble("shieldCon.Thick");

    // The iron cosmic ray shield.
    double shieldFeThick     = CLHEP::mm * _config->getDouble("shieldFe.thick");
    double shieldFeOuterHW   = CLHEP::mm * _config->getDouble("shieldFe.outerHalfWidth");
    double shieldFeZExtra    = CLHEP::mm * _config->getDouble("shieldFe.zextra");
    
    // Materials for the above.
    G4Material* shieldConMaterial       = materialFinder.get("shieldCon.materialName");
    G4Material* shieldConInsideMaterial = materialFinder.get("shieldCon.insideMaterialName");
    G4Material* shieldFeMaterial        = materialFinder.get("shieldFe.materialName");
    G4Material* shieldFeInsideMaterial  = materialFinder.get("shieldFe.insideMaterialName");
    
    // Derived information for the concrete and Fe shields.
    double shieldConInsideHalfDim[3], shieldConOutsideHalfDim[3];
    double shieldFeInsideHalfDim[3], shieldFeOutsideHalfDim[3];
    
    shieldConOutsideHalfDim[0] = shieldFeOuterHW + shieldConXSpace + shieldConThick;
    shieldConOutsideHalfDim[1] = ( shieldConInsideHeight + shieldConThick)/2.;
    
    shieldConInsideHalfDim[0]  = shieldFeOuterHW + shieldConXSpace;
    shieldConInsideHalfDim[1]  = shieldConInsideHeight/2.;

    shieldFeOutsideHalfDim[0]  = shieldFeOuterHW;
    shieldFeOutsideHalfDim[1]  = shieldFeOuterHW;

    shieldFeInsideHalfDim[0]   = shieldFeOuterHW - shieldFeThick;
    shieldFeInsideHalfDim[1]   = shieldFeOuterHW - shieldFeThick;

    shieldFeInsideHalfDim[2]  = dsHalfLength + shieldFeZExtra;
    shieldFeOutsideHalfDim[2]  = shieldFeInsideHalfDim[2];

    shieldConInsideHalfDim[2] = shieldConInsideHalfLength;
    shieldConOutsideHalfDim[2] = shieldConInsideHalfLength + shieldConThick;

    // Position of concrete box inside the air volume of the hall.
    G4ThreeVector shieldConOffset = G4ThreeVector(
                                                  detSolXoff-hallPosition[0],
                                                  shieldConOutsideHalfDim[1] - hallInHLen[1],
                                                  dsz0+_mu2eOrigin.z()
                                                  );

    // Position of air inside concrete shield wrt concrete.
    double yoff2 = shieldConInsideHalfDim[1] - shieldConOutsideHalfDim[1];
    G4ThreeVector shieldConInsideOffset = G4ThreeVector(0.,yoff2,0.);

    // Position of the iron shield inside wrt the air inside the concrete.
    G4ThreeVector shieldFeOffset( 0., yOriginHeight-shieldConInsideHalfDim[1], 0. );

    // Concrete shield around DS.
    VolumeInfo shieldConInfo = nestBox( "ShieldConDS_01",
                                        shieldConOutsideHalfDim,
                                        shieldConMaterial,
                                        0,
                                        shieldConOffset,
                                        hallInfo.logical,
                                        0,
                                        G4Colour::Blue()
                                        );

    // Air between the concrete and Fe shields.
    VolumeInfo shieldConInsideInfo = nestBox( "ShieldConDS_01_AIR",
                                              shieldConInsideHalfDim,
                                              shieldConInsideMaterial,
                                              0,
                                              shieldConInsideOffset,
                                              shieldConInfo.logical,
                                              0,
                                              G4Colour::Blue()
                                              );
    
    // Fe shield around DS.
    VolumeInfo shieldFeInfo = nestBox( "ShieldFe_01",
                                       shieldFeOutsideHalfDim,
                                       shieldFeMaterial,
                                       0,
                                       shieldFeOffset,
                                       shieldConInsideInfo.logical,
                                       0,
                                       G4Colour::Green()
                                       );
    
    // Air between the Fe shield and the DS cryostat.
    VolumeInfo shieldFeInsideInfo = nestBox( "ShieldFe_AIR_01",
                                             shieldFeInsideHalfDim,
                                             shieldFeInsideMaterial,
                                             0,
                                             G4ThreeVector(),
                                             shieldFeInfo.logical,
                                             0,
                                             G4Colour::Green()
                                             );
    
    // this was supposed to be in/out,  but it looks like toyDS.rOut and rIn are reversed, 
    //so I will put them in the wrong order.  kutschke agrees was a bug.
    double detSolCoilParams[5] = { 
      _config->getDouble("toyDS.rIn"       ) * CLHEP::mm,
      _config->getDouble("toyDS.rOut"      ) * CLHEP::mm,
      _config->getDouble("toyDS.halfLength") * CLHEP::mm,
      0.,
      2.*M_PI
    };
 
    //will divide the detector solenoid vacuum into two parts. the purpose
    //is to have a slowly falling field (the real one) over the stopping target
    //region, and then a pure solenoidal field in the region of the tracker.
    //
    //this will greatly simplify debugging of the kalman filter and tracking algorithms.
    //I considered fudging the field Julie Managan put in, which is the upstream half; Kutschke
    //says this will cause problems because no matter what I do it will violate Poisson's equation
    //at some point.  By dividing it into two parts, I can attach different fieldMgrs to the two sections
    //and this does less emotional violence to G4 as long as I choose a nice transition point.
    // rhb 1/22/10
    //
    //this transitionZ is in theory the only hardwired fudge to make this work; then calculate everything
    //given where we choose to move from one fieldMgr to the other

    //
    //ok, now compute the new halfLengths and centers; note Rob's code forces center at zero locally but give it a name for readability
    // we need this here because we're cutting the length of the ds vac by this amount 
    ///////////////////////////
      ///////////////////////////
      ///////////////////////////

      // Half Length of the block to prevent leakage of vaccume
      // This block is covering TS1 and placed between TS1-coil & DS-Upstream coil
      // Right now this is inside the DS  
      double toyDSBK1dothalfLength = 500.0*CLHEP::mm;   //this length can vary 
      //
      //these are TS1 coil parameters
      double toyTS1dotrIn  = 600.0*CLHEP::mm;    
      double toyTS1dotrOut = 700.0*CLHEP::mm;    
      double toyTS1dothalfLength = 500.0*CLHEP::mm;

      double centerOfDS = 0.;
      //now, you can read off a transition Z based on the field map and translate it to the local system
      double globalTransitionZ = dsz0; //so let's start at local 0 for debugging
      double transitionZ = (globalTransitionZ - dsz0) - 1500.;//transition arbitrary while I debug but must respect target/tracker locations
    
      double halfLengthOfUpstreamDSVac     = (_config->getDouble("toyDS.halfLengthVac")*CLHEP::mm + (transitionZ-centerOfDS))/2.
	- toyDSBK1dothalfLength - toyTS1dothalfLength;
      double halfLengthOfDownstreamDSVac   = _config->getDouble("toyDS.halfLengthVac")*CLHEP::mm - halfLengthOfUpstreamDSVac;
      //and of course the center of the upstream and downstream sections just moved
      double centerOfUpstreamDSVac   = transitionZ - halfLengthOfUpstreamDSVac;
      double centerOfDownstreamDSVac = transitionZ + halfLengthOfDownstreamDSVac;

      //print out all this nonsense
      /*
	cout << "we started with a center at: " << globalTransitionZ - dsz0
	<< " and a halflength of " << _config->getDouble("toyDS.halfLengthVac") 
	<< " a transition z at " << transitionZ 
	<< " and ended up with " << endl;

	cout << " halfLengthOfUpstreamDSVac = " << halfLengthOfUpstreamDSVac
	<< " centerOfUpstreamDSVac = " << centerOfUpstreamDSVac
	<< endl;
	cout << " halfLengthOfDownstreamDSVac = " << halfLengthOfDownstreamDSVac
	<< " centerOfDownstreamDSVac = " << centerOfDownstreamDSVac
	<< endl;
      */
      //checked this out for a special case, looked fine
      //     assert(2==1);

      double detSolVacParams[5] = { 
	0. * CLHEP::mm,
	detSolCoilParams[0],
	_config->getDouble("toyDS.halfLengthVac") * CLHEP::mm,
	0.,
	2.*M_PI
      };

      //cout << "toyDS.halfLengthVac = " << _config->getDouble("toyDS.halfLengthVac") << endl;
      //    assert (2==1);
      double detSolDownstreamVacParams[5]   = { 
	0.
	,detSolCoilParams[0]
	,halfLengthOfDownstreamDSVac*CLHEP::mm
	,0.
	,2.*M_PI
      };
      G4Material* detSolCoilMaterial = materialFinder.get("toyDS.materialName");
      //no longer used    G4Material* detSolVacMaterial  = materialFinder.get("toyDS.insideMaterialName");
      G4Material* detSolUpstreamVacMaterial    = materialFinder.get("toyDS.insideMaterialName");
      G4Material* detSolDownstreamVacMaterial  = materialFinder.get("toyDS.insideMaterialName");
    
      // Toy model of the DS coils + cryostat. It needs more structure and has
      // much less total material.
      VolumeInfo detSolCoilInfo = nestTubs( "ToyDSCoil",
					    detSolCoilParams,
					    detSolCoilMaterial,
					    0,
					    G4ThreeVector(),
					    shieldFeInsideInfo.logical,
					    0,
					    G4Color::Magenta()
					    );
      /* again not used with split 
      // The vacuum inside the DS cryostat and coils; this is longer in z than the coils+cryo.
      VolumeInfo detSolVacInfo = nestTubs( "ToyDSVacuum",
      detSolVacParams,
      detSolVacMaterial,
      0,
      G4ThreeVector(),
      shieldFeInsideInfo.logical,
      0,
      G4Color::Magenta()
      );
      */


      G4ThreeVector detSolDownstreamOffset  = G4ThreeVector(0.,0.,centerOfDownstreamDSVac);
      VolumeInfo detSolDownstreamVacInfo = nestTubs( "ToyDSDownstreamVacuum",
						     detSolDownstreamVacParams,
						     detSolDownstreamVacMaterial,
						     0,
						     detSolDownstreamOffset,
						     shieldFeInsideInfo.logical,
						     0,
						     G4Color::Magenta()
						     );







      double detSolUpstreamVacParams[5]   = {
	0.
	,detSolCoilParams[0]
	//      ,halfLengthOfUpstreamDSVac - toyDSBK1dothalfLength
	,halfLengthOfUpstreamDSVac
	,0.
	,2.*M_PI
      };
      //    G4ThreeVector detSolUpstreamOffset  = G4ThreeVector(detSolXoff-hallPosition[0],
      //					       shieldConOutsideHalfDim[1] - hallInHLen[1],
      //		   	  dsz0+_mu2eOrigin.z() + centerOfUpstreamDSVac*CLHEP::mm + toyDSBK1dothalfLength);
      G4ThreeVector detSolUpstreamOffset  = G4ThreeVector(0.,0.,
							  centerOfUpstreamDSVac);
      VolumeInfo detSolUpstreamVacInfo   = nestTubs( "ToyDSUpstreamVacuum",
						     detSolUpstreamVacParams,
						     detSolUpstreamVacMaterial,
						     0,
						     detSolUpstreamOffset,
						     shieldFeInsideInfo.logical,
						     0,
						     G4Colour::Yellow(), //color change between two halves
						     0
						     );



      //Now make TS1 coil with respect to hall
      //radius and halfLength are already defiend
      //    G4ThreeVector TS1CoilOffset = G4ThreeVector(detSolXoff-hallPosition[0],
      //					       shieldConOutsideHalfDim[1] - hallInHLen[1],
      //dsz0+_mu2eOrigin.z() +centerOfUpstreamDSVac*CLHEP::mm -halfLengthOfUpstreamDSVac*CLHEP::mm + 2*toyDSBK1dothalfLength - toyTS1dothalfLength);

      //
      // same mother volume as detector solenoid, you need this offset
      G4ThreeVector TS1CoilOffset = G4ThreeVector(0.,0.,
						  centerOfUpstreamDSVac -
						  (halfLengthOfUpstreamDSVac + 2*toyDSBK1dothalfLength + toyTS1dothalfLength));
      double TS1CoilParams[5] = { 
	toyTS1dotrIn,
	toyTS1dotrOut,
	toyTS1dothalfLength,
	0.,
	2.*M_PI
      };
      G4Material* TS1CoilMaterial  = materialFinder.get("toyDS.materialName");
      VolumeInfo TS1CoilInfo = nestTubs( "ToyTS1Coil",
					 TS1CoilParams,
					 TS1CoilMaterial,
					 0,
					 TS1CoilOffset,
					 //
					 // make this have same mother as DS
					 //                                           hallInfo.logical,
					 shieldFeInsideInfo.logical,
					 0,
					 G4Color::White(),
					 1
					 );

      //This is TS1 vacuum with respect to TS1 coil
      G4ThreeVector TS1VacOffset = G4ThreeVector();
      double TS1VacParams[5] = { 
	0.0,
	toyTS1dotrIn,
	toyTS1dothalfLength,
	0.,
	2.*M_PI
      };
      G4Material* TS1VacMaterial  = materialFinder.get("toyDS.insideMaterialName");
      VolumeInfo TS1VacInfo = nestTubs( "ToyTS1Vacuum",
					TS1VacParams,
					TS1VacMaterial,
					0,
					TS1VacOffset,
					TS1CoilInfo.logical,
					0,
					G4Color::Yellow(),
					1
					);



      //this is block around TS1 to prevent vacuum leakage
      if(toyDSBK1dothalfLength!=0){ //length of block should not be zero!
	//      G4ThreeVector DSBK1CoilOffset =  G4ThreeVector(detSolXoff-hallPosition[0],
	//						     shieldConOutsideHalfDim[1] - hallInHLen[1],
	// dsz0+_mu2eOrigin.z() +centerOfUpstreamDSVac*CLHEP::mm -halfLengthOfUpstreamDSVac*CLHEP::mm +toyDSBK1dothalfLength);

	//
	// placing this inside same mother volume as detector solenoid, you need this offset
	G4ThreeVector DSBK1CoilOffset =  G4ThreeVector(0.,0.,
						       centerOfUpstreamDSVac -(halfLengthOfUpstreamDSVac +toyDSBK1dothalfLength));

	double DSBK1CoilParams[5] = { 
	  toyTS1dotrOut,
	  _config->getDouble("toyDS.rIn")*CLHEP::mm,
	  toyDSBK1dothalfLength,
	  0.,
	  2.*M_PI
	};
	G4Material* DSBK1CoilMaterial  = materialFinder.get("toyDS.materialName");
	VolumeInfo DSBK1CoilInfo = nestTubs( "ToyDSBK1Coil",
					     DSBK1CoilParams,
					     DSBK1CoilMaterial,
					     0,
					     DSBK1CoilOffset,
					     shieldFeInsideInfo.logical,
					     0,
					     G4Color::Green(),
					     1
					     );
      }//length of block should not be zero!


      //this is TS3 coil parameters
      double toyTS3dotrIn  = 600.0*CLHEP::mm;    
      double toyTS3dotrOut = 700.0*CLHEP::mm;    
      double toyTS3dothalfLength = 1950.0*CLHEP::mm/2.0;


      //this is TS2 coil parameters
      double toyTS2dotrIn  = 600.0*CLHEP::mm;    
      double toyTS2dotrOut = 700.0*CLHEP::mm;    
      double toyTS2dothalfLength = -detSolXoff - toyTS3dothalfLength;


      //Now make TS2 coil
      G4ThreeVector TS2CoilOffset =  G4ThreeVector(-hallPosition[0] -toyTS3dothalfLength, yOriginHeight -hallInHLen[1], _mu2eOrigin.z() +toyTS2dothalfLength);
      double TS2CoilParams[5] = { 
	toyTS2dotrIn,
	toyTS2dotrOut,
	toyTS2dothalfLength,
	0.5*M_PI,
	0.5*M_PI
      };
      G4Material* TS2CoilMaterial  = materialFinder.get("toyDS.materialName");
      G4RotationMatrix* TS2CoilRot = new G4RotationMatrix();
      TS2CoilRot->rotateX(90.0*CLHEP::degree);
      VolumeInfo TS2CoilInfo = nestTorus("ToyTS2Coil",
					 TS2CoilParams,
					 TS2CoilMaterial,
					 TS2CoilRot,
					 TS2CoilOffset,
					 hallInfo.logical,
					 0,
					 G4Color::Red(),
					 1
					 );


      //Now make TS2 vacuum
      G4ThreeVector TS2VacOffset =  G4ThreeVector();
      double TS2VacParams[5] = { 
	0.0,
	toyTS2dotrIn,
	toyTS2dothalfLength,
	0.5*M_PI,
	0.5*M_PI
      };
      G4Material* TS2VacMaterial  = materialFinder.get("toyDS.insideMaterialName");
      G4RotationMatrix* TS2VacRot = new G4RotationMatrix();
      TS2VacRot->rotateX(0.0*CLHEP::degree);
      VolumeInfo TS2VacInfo = nestTorus("ToyTS2Vac",
					TS2VacParams,
					TS2VacMaterial,
					TS2VacRot,
					TS2VacOffset,
					TS2CoilInfo.logical,
					0,
					G4Color::Yellow(),
					1
					);



      //this is TS4 coil parameters
      double toyTS4dotrIn  = 600.0;    
      double toyTS4dotrOut = 700.0;    
      double toyTS4dothalfLength = -detSolXoff - toyTS3dothalfLength;

      //Now make TS4 coil
      G4ThreeVector TS4CoilOffset =  G4ThreeVector(-hallPosition[0] + toyTS3dothalfLength, yOriginHeight-hallInHLen[1], _mu2eOrigin.z() - toyTS2dothalfLength);
      double TS4CoilParams[5] = { 
	toyTS4dotrIn*CLHEP::mm,
	toyTS4dotrOut*CLHEP::mm,
	toyTS4dothalfLength*CLHEP::mm,
	1.5*M_PI,
	0.5*M_PI
      };
      G4Material* TS4CoilMaterial  = materialFinder.get("toyDS.materialName");
      G4RotationMatrix* TS4CoilRot = new G4RotationMatrix();
      TS4CoilRot->rotateX(90.0*CLHEP::degree);
      VolumeInfo TS4CoilInfo = nestTorus("ToyTS4Coil",
					 TS4CoilParams,
					 TS4CoilMaterial,
					 TS4CoilRot,
					 TS4CoilOffset,
					 hallInfo.logical,
					 0,
					 G4Color::Red(),
					 1
					 );


      //Now make TS4 vacuum
      G4ThreeVector TS4VacOffset =  G4ThreeVector();
      double TS4VacParams[5] = { 
	0.0,
	toyTS4dotrIn,
	toyTS4dothalfLength,
	1.5*M_PI,
	0.5*M_PI
      };
      G4Material* TS4VacMaterial  = materialFinder.get("toyDS.insideMaterialName");
      G4RotationMatrix* TS4VacRot = new G4RotationMatrix();
      TS2VacRot->rotateX(0.0*CLHEP::degree);
      VolumeInfo TS4VacInfo = nestTorus("ToyTS4Vac",
					TS4VacParams,
					TS4VacMaterial,
					TS4VacRot,
					TS4VacOffset,
					TS4CoilInfo.logical,
					0,
					G4Color::Yellow(),
					1
					);



      //Now make TS3 coil
      G4ThreeVector TS3CoilOffset = G4ThreeVector(-hallPosition[0], yOriginHeight-hallInHLen[1], _mu2eOrigin.z() );
      double TS3CoilParams[5] = { 
	toyTS3dotrIn*CLHEP::mm,
	toyTS3dotrOut*CLHEP::mm,
	toyTS3dothalfLength*CLHEP::mm,
	0.,
	2.*M_PI
      };
      G4Material* TS3CoilMaterial  = materialFinder.get("toyDS.materialName");
      G4RotationMatrix* TS3CoilRot = new G4RotationMatrix();
      TS3CoilRot->rotateY(90.*CLHEP::degree);
      VolumeInfo TS3CoilInfo = nestTubs( "ToyTS3Coil",
					 TS3CoilParams,
					 TS3CoilMaterial,
					 TS3CoilRot,
					 TS3CoilOffset,
					 hallInfo.logical,
					 0,
					 G4Color::White(),
					 1
					 );

      //This is TS1 vacuum
      G4ThreeVector TS3VacOffset = G4ThreeVector();
      double TS3VacParams[5] = { 
	0.0*CLHEP::mm,
	toyTS3dotrIn*CLHEP::mm,
	toyTS3dothalfLength*CLHEP::mm,
	0.,
	2.*M_PI
      };
      G4Material* TS3VacMaterial  = materialFinder.get("toyDS.insideMaterialName");
      VolumeInfo TS3VacInfo = nestTubs( "ToyTS3Vacuum",
					TS3VacParams,
					TS3VacMaterial,
					0,
					TS3VacOffset,
					TS3CoilInfo.logical,
					0,
					G4Color::Yellow(),
					1
					);
    
      ///////////////////////////
	///////////////////////////
	///////////////////////////










	// Mock up of the production solenoid and its vacuum.

	double prodSolCoilParams[5] = { 
	  _config->getDouble("toyPS.rIn"       ) * CLHEP::mm,
	  _config->getDouble("toyPS.rOut"      ) * CLHEP::mm,
	  _config->getDouble("toyPS.halfLength") * CLHEP::mm,
	  0.,
	  2.*M_PI
	};

	G4Material* prodSolCoilMaterial = materialFinder.get("toyPS.materialName");   
	// Position of PS inside the air volume of the hall.
	G4ThreeVector prodSolCoilOffset = 
	  G4ThreeVector( prodSolXoff-hallPosition[0],
			 yOriginHeight - hallInHLen[1],
			 _config->getDouble("toyPS.z0") * CLHEP::mm + _mu2eOrigin.z()
			 );

	// Toy model of the PS coils + cryostat. It needs more structure and has
	// much less total material.
	VolumeInfo prodSolCoilInfo = nestTubs( "ToyPSCoil",
					       prodSolCoilParams,
					       prodSolCoilMaterial,
					       0,
					       prodSolCoilOffset,
					       hallInfo.logical,
					       0,
					       G4Color::Cyan(),
					       1
					       );
    


	///////////////////////////
	  ///////////////////////////
	  ///////////////////////////

	  // Half Length of the block to prevent leakage of vaccume
	  // This block is covering TS1 and placed between TS1-coil & PS-coil 
	  double toyPSBK1dothalfLength = 50.0*CLHEP::mm; 

	  //this is TS5 coil parameters
	  double toyTS5dotrIn  = 600.0*CLHEP::mm;    
	  double toyTS5dotrOut = 700.0*CLHEP::mm;    
	  double toyTS5dothalfLength =  500.0*CLHEP::mm;


	  // production solenoid vacuum recreated with respect to block
	  G4ThreeVector prodSolVacOffset = G4ThreeVector(prodSolXoff - hallPosition[0],
							 yOriginHeight - hallInHLen[1],
							 _config->getDouble("toyPS.z0") * CLHEP::mm + _mu2eOrigin.z() - toyPSBK1dothalfLength); 
	  double prodSolVacParams[5] = { 
	    0.*CLHEP::mm,
	    prodSolCoilParams[0],
	    _config->getDouble("toyPS.halfLengthVac")*CLHEP::mm - toyPSBK1dothalfLength,
	    0.,
	    2.*M_PI
	  };
	  G4Material* prodSolVacMaterial  = materialFinder.get("toyPS.insideMaterialName");
	  VolumeInfo prodSolVacInfo = nestTubs( "ToyPSVacuum",
						prodSolVacParams,
						prodSolVacMaterial,
						0,
						prodSolVacOffset,
						hallInfo.logical,
						0,
						G4Color::Yellow(),
						0
						);





	  //Now make TS5 coil
	  G4ThreeVector TS5CoilOffset = G4ThreeVector(prodSolXoff - hallPosition[0],
						      yOriginHeight - hallInHLen[1],
						      _config->getDouble("toyPS.z0")*CLHEP::mm +_mu2eOrigin.z() +_config->getDouble("toyPS.halfLength")*CLHEP::mm -2.0*toyPSBK1dothalfLength +toyTS5dothalfLength);     
	  double TS5CoilParams[5] = { 
	    toyTS5dotrIn,
	    toyTS5dotrOut,
	    toyTS5dothalfLength,
	    0.,
	    2.*M_PI
	  };
	  G4Material* TS5CoilMaterial  = materialFinder.get("toyPS.materialName");
	  VolumeInfo TS5CoilInfo = nestTubs( "ToyTS5Coil",
					     TS5CoilParams,
					     TS5CoilMaterial,
					     0,
					     TS5CoilOffset,
					     hallInfo.logical,
					     0,
					     G4Color::White(),
					     1
					     );

	  //This is TS5 vacuum
	  G4ThreeVector TS5VacOffset = G4ThreeVector();
	  double TS5VacParams[5] = { 
	    0.0,
	    toyTS5dotrIn,
	    toyTS5dothalfLength,
	    0.,
	    2.*M_PI
	  };
	  G4Material* TS5VacMaterial  = materialFinder.get("toyPS.insideMaterialName");
	  VolumeInfo TS5VacInfo = nestTubs( "ToyTS5Vacuum",
					    TS5VacParams,
					    TS5VacMaterial,
					    0,
					    TS5VacOffset,
					    TS5CoilInfo.logical,
					    0,
					    G4Color::Yellow(),
					    1
					    );
    


    
	  //this is block around TS5 to prevent vacuum leakage
	  if(toyPSBK1dothalfLength!=0){//only when vacuum leak block is non-zero lenght
	    G4ThreeVector PSBK1CoilOffset = G4ThreeVector(prodSolXoff - hallPosition[0],
							  yOriginHeight - hallInHLen[1],
							  _config->getDouble("toyPS.z0")*CLHEP::mm +_mu2eOrigin.z() +_config->getDouble("toyPS.halfLength")*CLHEP::mm -toyPSBK1dothalfLength);     


	    double PSBK1CoilParams[5] = { 
	      toyTS5dotrOut,
	      _config->getDouble("toyPS.rIn")*CLHEP::mm,
	      toyPSBK1dothalfLength,
	      0.,
	      2.*M_PI
	    };
	    G4Material* PSBK1CoilMaterial  = materialFinder.get("toyPS.materialName");
	    VolumeInfo PSBK1CoilInfo = nestTubs( "ToyPSBK1Coil",
						 PSBK1CoilParams,
						 PSBK1CoilMaterial,
						 0,
						 PSBK1CoilOffset,
						 hallInfo.logical,
						 0,
						 G4Color::Magenta(),
						 1
						 );
	  }//only when vacuum leak block is non-zero lenght


	  ///////////////////////////
	    ///////////////////////////
	    ///////////////////////////





















	    // Proton Target in PS 

	    // Proton Target parameters 
	    // Proton Target position
	    G4ThreeVector ProtonTargetPosition = G4ThreeVector( 
							       _config->getDouble("targetPS_positionX")*CLHEP::mm,
							       _config->getDouble("targetPS_positionY")*CLHEP::mm,
							       _config->getDouble("targetPS_positionZ")*CLHEP::mm
							       );
    
	    // Rotation of Proton Target                                
	    double targetPS_rotX = _config->getDouble("targetPS_rotX" );
	    double targetPS_rotY = _config->getDouble("targetPS_rotY" );
    
	    // Proton Target Material
	    G4Material* targetPS_materialName = materialFinder.get("targetPS_materialName");
    
	    //Proton Target geometry parameters
	    double targetPS_Pam[5] = { 
	      0.,
	      _config->getDouble("targetPS_rOut"      ) * CLHEP::mm,
	      _config->getDouble("targetPS_halfLength") * CLHEP::mm,
	      0.,
	      2.*M_PI
	    };

	    // Rotation of Proton Target
	    G4RotationMatrix* PS_target_rot = new G4RotationMatrix();
	    PS_target_rot->rotateX( targetPS_rotX*CLHEP::degree);
	    PS_target_rot->rotateY( targetPS_rotY*CLHEP::degree);
   
	    //
	    // Creating Proton Target object in prodSolVacInfo logical object (v. khalatian)
	    VolumeInfo ProtonTargetInfo = nestTubs( "ProtonTarget",
						    targetPS_Pam,
						    targetPS_materialName,
						    PS_target_rot,
						    ProtonTargetPosition,
						    prodSolVacInfo.logical,
						    0,
						    G4Color::White()
						    );
   
	    // Primary Proton Gun Origin 
	    _primaryProtonGunOrigin = dirtOffset + wallOffset + hallOffset + prodSolCoilOffset + ProtonTargetPosition;
    
	    //Primary Proton Gun Rotation 
	    // For rotating Primary Proton Gun I take angles from Proton Target 
	    _primaryProtonGunRotation.rotateX( targetPS_rotX*CLHEP::degree);
	    _primaryProtonGunRotation.rotateY( targetPS_rotY*CLHEP::degree);
	    //
	    //these are "active rotations; we want passive, in G4 style
	    _primaryProtonGunRotation = _primaryProtonGunRotation.inverse();


	    // Construct one of the trackers.
	    VolumeInfo trackerInfo;
	    if( _config->getBool("hasLTracker",false) ){
	      int ver = _config->getInt("LTrackerVersion",1);
	      log << "LTracker version: " << ver << "\n";
	      if ( ver == 1 ){
		trackerInfo = constructLTrackerv1( detSolDownstreamVacInfo.logical, dsz0 + centerOfDownstreamDSVac, *_config );
	      }
	      else if ( ver == 2 ) {
		trackerInfo = constructLTrackerv2( detSolDownstreamVacInfo.logical, dsz0 + centerOfDownstreamDSVac, *_config );
	      } else {
		trackerInfo = constructLTrackerv3( detSolDownstreamVacInfo.logical, dsz0 + centerOfDownstreamDSVac, *_config );
	      }
	    } else if ( _config->getBool("hasITracker",false) ) {
	      trackerInfo = ITrackerBuilder::constructTracker( detSolDownstreamVacInfo.logical, dsz0 + centerOfDownstreamDSVac );
	    } else if ( _config->getBool("hasTTracker",false) ) {
	      trackerInfo = constructTTrackerv1( detSolDownstreamVacInfo.logical, dsz0 + centerOfDownstreamDSVac, *_config );
	    } else {
	      trackerInfo = constructDummyTracker( detSolDownstreamVacInfo.logical, dsz0 + centerOfDownstreamDSVac, *_config );
	    }

	    // 
	    VolumeInfo calorimeterInfo;
	    if ( _config->getBool("hasCalorimeter",false) ){
	      calorimeterInfo = constructCalorimeter( detSolDownstreamVacInfo.logical,
						      -(dsz0+centerOfDownstreamDSVac),
						      *_config );
	    }

	    // Do the Target
	    VolumeInfo targetInfo;
	    if( _config->getBool("hasTarget",false) ){

	      targetInfo = constructStoppingTarget( detSolUpstreamVacInfo.logical, 
						    dsz0 + centerOfUpstreamDSVac );

	    } else {

	      targetInfo = constructDummyStoppingTarget( detSolUpstreamVacInfo.logical, 
							 dsz0 + centerOfUpstreamDSVac,
							 *_config );
	    } //hasTarget


	    // Only after all volumes have been defined should we set the magnetic fields.
	    // Make the magnetic field valid inside the detSol vacuum; one upstream, one downstream

	    const char* fieldmap = "/home2/misc1/jmanagan/myMu2e/GMC/fieldmaps/dsmap_unfmt_rad100.dat";//note disgusting hardwired absolute path
	    int const nx(50); int const ny(25); int const nz(438);

	    G4double stepUpstreamMinimum(1.0e-2*CLHEP::mm);
	    G4double stepDownstreamMinimum(1.0e-2*CLHEP::mm);

	    //
	    //get the field form; default if unspecified is constant
	    int detSolFieldForm = _config->getInt("detSolFieldForm",detSolUpConstantDownConstant); 

	    cout << "detSolFieldForm  from Mu2eWorld.cc = " << detSolFieldForm << endl;
	    //assert (2==1);
	    // first check we have a legal configuration
	    if (detSolFieldForm != detSolFullField && detSolFieldForm != detSolUpVaryingDownConstant && detSolFieldForm != detSolUpConstantDownConstant)
	      {
		G4cout << " no legal field specification; detSolFieldForm = " << detSolFieldForm << G4endl;
		throw cms::Exception("GEOM")
		  << "illegal field config as specified in geom.txt \n";
	      }

	    if (detSolFieldForm == detSolFullField)
	      {
		//
		//upstream varying section

		_detSolUpstreamVaryingBField = auto_ptr<DSField>(new DSField(fieldmap,_mu2eOrigin,nx,ny,nz));
		_usualUpstreamRHS    = auto_ptr<G4Mag_UsualEqRhs>   (new G4Mag_UsualEqRhs( _detSolUpstreamVaryingBField.get() ) );
		_rungeEEUpstreamHelix  = auto_ptr<G4ExplicitEuler>(new G4ExplicitEuler(_usualUpstreamRHS.get()));
		_chordUpstreamFinder = auto_ptr<G4ChordFinder>      (new G4ChordFinder( _detSolUpstreamVaryingBField.get(), stepUpstreamMinimum
											, _rungeUpstreamHelix.get() ));
		_fieldUpstreamMgr    = auto_ptr<G4FieldManager>     (new G4FieldManager( _detSolUpstreamVaryingBField.get(), 
											 _chordUpstreamFinder.get(), true));
		//
		//downstream varying section
		_detSolDownstreamVaryingBField = auto_ptr<DSField>(new DSField(fieldmap,_mu2eOrigin,nx,ny,nz));
		_usualDownstreamRHS    = auto_ptr<G4Mag_UsualEqRhs>   (new G4Mag_UsualEqRhs( _detSolDownstreamVaryingBField.get() ) );
		_rungeEEDownstreamHelix  = auto_ptr<G4ExplicitEuler>(new G4ExplicitEuler(_usualDownstreamRHS.get()));
		_chordDownstreamFinder = auto_ptr<G4ChordFinder>      (new G4ChordFinder( _detSolDownstreamVaryingBField.get(), stepDownstreamMinimum,
											  _rungeDownstreamHelix.get() ));
		_fieldDownstreamMgr    = auto_ptr<G4FieldManager>     (new G4FieldManager( _detSolDownstreamVaryingBField.get(), 
											   _chordDownstreamFinder.get(), true));

	      }
	    if (detSolFieldForm == detSolUpVaryingDownConstant)
	      {
		cout << "in hybrid " << endl;
		//
		//upstream varying section
		_detSolUpstreamVaryingBField = auto_ptr<DSField>(new DSField(fieldmap,_mu2eOrigin,nx,ny,nz));
		_usualUpstreamRHS    = auto_ptr<G4Mag_UsualEqRhs>   (new G4Mag_UsualEqRhs( _detSolUpstreamVaryingBField.get()));
        
		_rungeEEUpstreamHelix  = auto_ptr<G4ExplicitEuler> (new G4ExplicitEuler(_usualUpstreamRHS.get()));
		_chordUpstreamFinder = auto_ptr<G4ChordFinder>      (new G4ChordFinder( _detSolUpstreamVaryingBField.get(), stepUpstreamMinimum
											, _rungeEEUpstreamHelix.get() ));
		_fieldUpstreamMgr    = auto_ptr<G4FieldManager>     (new G4FieldManager( _detSolUpstreamVaryingBField.get(), 
											 _chordUpstreamFinder.get(), true));

		//downstream constant section
		G4double bzDown = _config->getDouble("toyDS.bz") * tesla;
		_detSolDownstreamConstantBField = auto_ptr<G4UniformMagField>(new G4UniformMagField(G4ThreeVector(0.,0.,bzDown)));
		_usualDownstreamRHS    = auto_ptr<G4Mag_UsualEqRhs>   (new G4Mag_UsualEqRhs( _detSolDownstreamConstantBField.get()) );
		_exactDownstreamHelix  = auto_ptr<G4ExactHelixStepper>(new G4ExactHelixStepper(_usualDownstreamRHS.get()));
		_chordDownstreamFinder = auto_ptr<G4ChordFinder>      (new G4ChordFinder( _detSolDownstreamConstantBField.get(), stepDownstreamMinimum
											  , _exactDownstreamHelix.get() ));
		_fieldDownstreamMgr    = auto_ptr<G4FieldManager>     (new G4FieldManager( _detSolDownstreamConstantBField.get(), 
											   _chordDownstreamFinder.get(), true));
	      }

	    if (detSolFieldForm == detSolUpConstantDownConstant) 
	      {
		cout << "in constant field" << endl;
		//
		// constant field, but split into two parts; upstream first
		G4double bzUp = _config->getDouble("toyDS.bz") * tesla;
		_detSolUpstreamConstantBField = auto_ptr<G4UniformMagField>(new G4UniformMagField(G4ThreeVector(0.,0.,bzUp)));
		_usualUpstreamRHS    = auto_ptr<G4Mag_UsualEqRhs>   (new G4Mag_UsualEqRhs( _detSolUpstreamConstantBField.get() ) );
		_exactUpstreamHelix  = auto_ptr<G4ExactHelixStepper>(new G4ExactHelixStepper(_usualUpstreamRHS.get()));
		_chordUpstreamFinder = auto_ptr<G4ChordFinder>      (new G4ChordFinder( _detSolUpstreamConstantBField.get(), stepUpstreamMinimum
											, _exactUpstreamHelix.get() ));
		_fieldUpstreamMgr    = auto_ptr<G4FieldManager>     (new G4FieldManager( _detSolUpstreamConstantBField.get(), 
											 _chordUpstreamFinder.get(), true));
		//downstream
		G4double bzDown = _config->getDouble("toyDS.bz") * tesla;
		_detSolDownstreamConstantBField = auto_ptr<G4UniformMagField>(new G4UniformMagField(G4ThreeVector(0.,0.,bzDown)));
		_usualDownstreamRHS    = auto_ptr<G4Mag_UsualEqRhs>   (new G4Mag_UsualEqRhs( _detSolDownstreamConstantBField.get() ) );
		_exactDownstreamHelix  = auto_ptr<G4ExactHelixStepper>(new G4ExactHelixStepper(_usualDownstreamRHS.get()));
		_chordDownstreamFinder = auto_ptr<G4ChordFinder>      (new G4ChordFinder( _detSolDownstreamConstantBField.get(), stepDownstreamMinimum
											  , _exactDownstreamHelix.get() ));
		_fieldDownstreamMgr    = auto_ptr<G4FieldManager>     (new G4FieldManager( _detSolDownstreamConstantBField.get(), 
											   _chordDownstreamFinder.get(), true));
	      }



	    // Now that we've chosen, attach the field manager to the detSol volume; full field upstream
	    detSolUpstreamVacInfo.logical->SetFieldManager( _fieldUpstreamMgr.get(), true);
	    detSolDownstreamVacInfo.logical->SetFieldManager( _fieldDownstreamMgr.get(), true);



	    //
	    //set integration step values
	    G4double singleValue = 0.5e-01*CLHEP::mm;
	    G4double newUpstreamDeltaI = singleValue;
	    G4double newDownstreamDeltaI = singleValue;
	    G4double deltaOneStep = singleValue;
	    G4double deltaChord = singleValue;
	    G4double maxStep = 20.e-00*CLHEP::mm;

	    // Leave the defaults for the uniform field; override them for non-uniform field.
	    if ( detSolFieldForm == detSolFullField || detSolFieldForm == detSolUpVaryingDownConstant ){
	      _fieldUpstreamMgr->SetDeltaOneStep(deltaOneStep);
	      _fieldUpstreamMgr->SetDeltaIntersection(newUpstreamDeltaI);    
	      _chordUpstreamFinder->SetDeltaChord(deltaChord);
	    }
	    if ( detSolFieldForm == detSolFullField ){
	      _fieldDownstreamMgr->SetDeltaOneStep(deltaOneStep);
	      _fieldDownstreamMgr->SetDeltaIntersection(newDownstreamDeltaI);    
	      _chordDownstreamFinder->SetDeltaChord(deltaChord);
	    }

	    // For the uniform field, change only deltaIntersection.
	    if ( detSolFieldForm == detSolUpConstantDownConstant ||
		 detSolFieldForm == detSolUpVaryingDownConstant     ){
	      G4double deltaIntersection = 0.00001*CLHEP::mm;
	      if ( detSolFieldForm == detSolUpConstantDownConstant ){
		_fieldUpstreamMgr->SetDeltaIntersection(deltaIntersection);
	      }
	      _fieldDownstreamMgr->SetDeltaIntersection(deltaIntersection);    
	    }
    
	    // Set step limit.  
	    // See also PhysicsList.cc to add a steplimiter to the list of processes.
	    // Do this so that we can see the helical trajectory in the DS and volumes inside of it.
	    _stepLimit = auto_ptr<G4UserLimits>( new G4UserLimits(maxStep));
	    detSolUpstreamVacInfo.logical->SetUserLimits(_stepLimit.get());
	    detSolDownstreamVacInfo.logical->SetUserLimits(_stepLimit.get());

	    trackerInfo.logical->SetUserLimits(_stepLimit.get());
	    targetInfo.logical->SetUserLimits(_stepLimit.get());

  }

  // Convert to base units for all of the items in the vector.
  void Mu2eWorld::setUnits( vector<double>& V, G4double unit ){
    for ( vector<double>::iterator b=V.begin(), e=V.end();
          b!=e; ++b){
      *b *= unit;
    }
  }

} // end namespace mu2e
