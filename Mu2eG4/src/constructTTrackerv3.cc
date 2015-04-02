//
// Free function to construct version 3 of the TTracker
//
// $Id: constructTTrackerv3.cc,v 1.38 2014/04/11 04:42:06 genser Exp $
// $Author: genser $
// $Date: 2014/04/11 04:42:06 $
//
// Original author KLG based on RKK's version using different methodology
//
// Notes
//
// 1)  The v3 in this function name says that this is the third way we
//     have implemented a single TTracker design in G4.  It does not refer
//     to alternate designs of the TTracker.
//
//     This version makes logical mother volumes per device and per
//     sector and places sectors in device and straws in sector
//     It has only one sector/device logical volume placed several times
//     This version has a negligeable construction time and a much smaler memory footprint
//
// 2) This function can build the TTracker designs described in:
//      Mu2eG4/test/ttracker_meco.txt - The MECO design, uniform plane spacing
//      Mu2eG4/test/ttracker_v0.txt   - The first Aseet version, pairs of planes form stations
//                                      but one layer of straws per panel (called a sector in this code)
//      Mu2eG4/test/ttracker_v1.txt   - v0 but with with two layers of straws per panel
//      Mu2eG4/test/ttracker_v2.txt   - Adjust spacings to match Mu2e-doc-888-v2.
//
// 3) This function does not know how to build the TTracker described in:
//       Mu2eG4/test/ttracker_v3.txt - Detail support model and detailed layering of straws
//    This geometry can be detected by the method by
//
//      if ( ttracker.getSupportModel() == SupportModel::detailedv0 ) ....
//
//    If this geometry is detected, this function call through to constructTTrackerv3Detailed.cc

// C++ includes
#include <iomanip>
#include <iostream>
#include <string>

// Framework includes
#include "cetlib/exception.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

// Mu2e includes
#include "G4Helper/inc/G4Helper.hh"
#include "GeometryService/inc/GeomHandle.hh"
#include "Mu2eG4/inc/SensitiveDetectorName.hh"
#include "Mu2eG4/inc/StrawSD.hh"
#include "Mu2eG4/inc/constructTTracker.hh"
#include "Mu2eG4/inc/ConstructTTrackerTDR.hh"
#include "Mu2eG4/inc/findMaterialOrThrow.hh"
#include "Mu2eG4/inc/finishNesting.hh"
#include "Mu2eG4/inc/nestTubs.hh"
#include "TTrackerGeom/inc/TTracker.hh"
#include "Mu2eG4/inc/checkForOverlaps.hh"

// G4 includes
#include "G4Box.hh"
#include "G4Colour.hh"
#include "G4IntersectionSolid.hh"
#include "G4Material.hh"
#include "G4PVPlacement.hh"
#include "G4SDManager.hh"
#include "G4String.hh"
#include "G4ThreeVector.hh"
#include "G4Trd.hh"
#include "G4Tubs.hh"


using namespace std;

namespace mu2e{

  VolumeInfo constructTTrackerv3( VolumeInfo const& ds3Vac,
                                  SimpleConfig const& config ){

    // Master geometry for the TTracker.
    TTracker const & ttracker = *(GeomHandle<TTracker>());

    // The more detailed version has its own function.
    if ( ttracker.getSupportModel() == SupportModel::detailedv0 ) {
      ConstructTTrackerTDR tt(ds3Vac, config);
      return tt.motherInfo();
    }

    G4Helper    & _helper = *(art::ServiceHandle<G4Helper>());
    AntiLeakRegistry & reg = _helper.antiLeakRegistry();

    int verbosityLevel = config.getInt("ttracker.verbosityLevel",0);

    // Control of graphics for debugging the geometry.
    // Only instantiate sectors to be drawn.
    int deviceDraw = config.getInt("ttracker.devDraw",-1);
    int sectorDraw = config.getInt("ttracker.secDraw",-1);
    bool doSurfaceCheck = config.getBool("g4.doSurfaceCheck",false) ||
      config.getBool("ttracker.doSurfaceCheck",false);
    bool const forceAuxEdgeVisible = config.getBool("g4.forceAuxEdgeVisible",false);

    G4ThreeVector const zeroVector(0.0,0.0,0.0);

    // The devices are now called planes in the CDR

    static int const newPrecision = 8;
    static int const newWidth = 14;

    // Parameters of the new style mother volume ( replaces the envelope volume ).
    PlacedTubs const& mother = ttracker.mother();

    // Offset of the center of the tracker within its mother volume.
    CLHEP::Hep3Vector motherOffset = mother.position() - ds3Vac.centerInWorld;

    if (verbosityLevel > 0) {
      int oldPrecision = cout.precision(newPrecision);
      int oldWidth = cout.width(newWidth);
      std::ios::fmtflags oldFlags = cout.flags();
      cout.setf(std::ios::fixed,std::ios::floatfield);
      cout << __func__ << " tracker mother tubsParams ir,or,zhl,phi0,phimax:            " <<
	"   " <<
	mother.tubsParams().innerRadius() << ", " <<
	mother.tubsParams().outerRadius() << ", " <<
	mother.tubsParams().zHalfLength() << ", " <<
	mother.tubsParams().phi0()        << ", " <<
	mother.tubsParams().phiMax()      << ", " <<
	endl;
      cout.setf(oldFlags);
      cout.precision(oldPrecision);
      cout.width(oldWidth);
    }

    // The z position of the tracker origin in the system attached to mother volume.
    CLHEP::Hep3Vector originOffset( 0., 0., ttracker.z0()-mother.position().z());

    G4Material* envelopeMaterial = findMaterialOrThrow(ttracker.envelopeMaterial());

    VolumeInfo motherInfo = nestTubs( "TrackerMother",
                                      mother.tubsParams(),
                                      envelopeMaterial,
                                      0,
                                      motherOffset,
                                      ds3Vac,
                                      0,
                                      config.getBool("ttracker.envelopeVisible",false),
                                      G4Colour::Blue(),
                                      config.getBool("ttracker.envelopeSolid",true),
                                      forceAuxEdgeVisible,
                                      true,
                                      doSurfaceCheck
                                      );

    if ( verbosityLevel > 0) {
      double zhl         = static_cast<G4Tubs*>(motherInfo.solid)->GetZHalfLength();
      double motherOffsetInMu2eZ = motherInfo.centerInMu2e()[CLHEP::Hep3Vector::Z];
      int oldPrecision = cout.precision(3);
      std::ios::fmtflags oldFlags = cout.flags();
      cout.setf(std::ios::fixed,std::ios::floatfield);
      cout << __func__ << " motherOffsetZ           in Mu2e    : " <<
        motherOffsetInMu2eZ << endl;
      cout << __func__ << " mother         Z extent in Mu2e    : " <<
        motherOffsetInMu2eZ - zhl << ", " << motherOffsetInMu2eZ + zhl << endl;
      cout.setf(oldFlags);
      cout.precision(oldPrecision);
    }

    TubsParams deviceEnvelopeParams = ttracker.getDeviceEnvelopeParams();

    bool ttrackerDeviceEnvelopeVisible = config.getBool("ttracker.deviceEnvelopeVisible",false);
    bool ttrackerDeviceEnvelopeSolid   = config.getBool("ttracker.deviceEnvelopeSolid",true);
    bool ttrackerSupportVisible        = config.getBool("ttracker.supportVisible",false);
    bool ttrackerSupportSolid          = config.getBool("ttracker.supportSolid",true);
    bool ttrackerSectorEnvelopeVisible = config.getBool("ttracker.sectorEnvelopeVisible",false);
    bool ttrackerSectorEnvelopeSolid   = config.getBool("ttracker.sectorEnvelopeSolid",true);
    bool ttrackerStrawVisible          = config.getBool("ttracker.strawVisible",false);
    bool ttrackerStrawSolid            = config.getBool("ttracker.strawSolid",true);

    // will construct one panel=sector in its nominal position
    // in the new language the device is called a plane ( with two faces ) 
    // then stations have n=2 planes

    // some specific g4 rotations related to the volume type and direction of their axis
    static double const tRAngle  = M_PI_2;
    static double const tRAngle2 = M_PI;
    CLHEP::HepRotationX RXForTrapezoids(tRAngle);
    CLHEP::HepRotationX RX2ForTrapezoids(tRAngle2);
    CLHEP::HepRotationY RYForTrapezoids(tRAngle);
    CLHEP::HepRotationZ RZForTrapezoids(tRAngle);

    VolumeInfo sectorInfo;

    const size_t idev0 = 0;

    const Device& device0 = ttracker.getDevice(idev0);

    // place straws etc... wrt the envelope

    // create a "sector" volume

    // Construct One sector logical volume (and then place it N times)

    const size_t isec0 = 0;

    const Sector& sector0 = device0.getSector(isec0);

    // constructing sector envelope

    // Make a logical volume for this sector,

    // G4IntersectionSolid of G4Box and G4Trd to avoid overlaps of two envelopes

    // reuse device attributes for now

    // get the length of the innermost straw
    Layer const& layer0        = sector0.getLayer(0);
    Straw const& straw0        = layer0.getStraw(0);
    StrawDetail const& detail0 = straw0.getDetail();

    verbosityLevel > 0 &&
      cout << __func__ << " sector box isec detail0.halfLength(): " << detail0.halfLength() << endl;


    sectorInfo.name = "TTrackerSectorEnvelope";

    G4Box* secBox = new G4Box(sectorInfo.name+"Box",
                              detail0.halfLength(),
                              sector0.boxHalfLengths()[2],
                              sector0.boxHalfLengths()[1]
                              );

    G4Trd* secTrd = new G4Trd(sectorInfo.name+"Trd",
                              sector0.boxHalfLengths()[4],
                              sector0.boxHalfLengths()[3],
                              sector0.boxHalfLengths()[2],
                              sector0.boxHalfLengths()[2],
                              sector0.boxHalfLengths()[1]
                              );

    // one could also intersect it with a ring to decrease its radial spread

    sectorInfo.solid =
      new G4IntersectionSolid(sectorInfo.name, secBox, secTrd);

    // false for placing physical volume, just create a logical one
    finishNesting(sectorInfo,
                  envelopeMaterial,
                  0,
                  zeroVector, // this is the "canonical" position, but it does not matter as there is no placement
                  0,
                  0,
                  ttrackerSectorEnvelopeVisible,
                  G4Colour::Cyan(),
                  ttrackerSectorEnvelopeSolid,
                  forceAuxEdgeVisible,
                  false, // only creating a logical volume
                  doSurfaceCheck
                  );

    if (verbosityLevel > 0 ){
      int oldPrecision = cout.precision(newPrecision);
      int oldWidth = cout.width(newWidth);
      std::ios::fmtflags oldFlags = cout.flags();
      cout.setf(std::ios::fixed,std::ios::floatfield);
      cout << __func__ << " sector box isec, sector.boxHalfLengths().at(4,3,2,2,1): " <<
        isec0 << ", " <<
        sector0.boxHalfLengths().at(4) << ", " <<
        sector0.boxHalfLengths().at(3) << ", " <<
        sector0.boxHalfLengths().at(2) << ", " <<
        sector0.boxHalfLengths().at(2) << ", " <<
        sector0.boxHalfLengths().at(1) << ", " <<
        endl;
      cout.setf(oldFlags);
      cout.precision(oldPrecision);
      cout.width(oldWidth);
    }

    // one has to "unrotate" the sector for the placements of the straws; see below 
    const CLHEP::HepRotationZ sector0RZRot(-sector0.boxRzAngle()); //It is arround z
    const G4ThreeVector unrotatedSector0Origin = sector0RZRot*sector0.boxOffset();

    G4RotationMatrix* rotTub = reg.add(G4RotationMatrix(RYForTrapezoids));

    for ( int ilay =0; ilay<sector0.nLayers(); ++ilay ){

      verbosityLevel > 1 &&   cout << __func__ << " constructTTrackerv3 ilay: " << ilay << endl;

      const Layer& layer = sector0.getLayer(ilay);

      for ( int istr=0; istr<layer.nStraws(); ++istr ){

        // "second" layer will have fewer straws (for now) also see TTrackerMaker
        // no, it complicates StrawSD and TTrackerMaker
        // if( ilay%2==1 && istr+1 == layer.nStraws() ) break;

        const Straw& straw = layer.getStraw(istr);

        StrawDetail const& detail = straw.getDetail();

        TubsParams strawWallParams( 0.0, detail.outerRadius(), detail.halfLength() );
        TubsParams strawGasParams ( 0.0, detail.innerRadius(), detail.halfLength() );
        TubsParams strawWireParams( 0.0, detail.wireRadius(),  detail.halfLength() );

        // we are placing the straw w.r.t the trapezoid...
        // the trapezoid aka device envelope has a different coordinate system x->z, z->y, y->x

        // this only works for "unrotated sector 0"; 
        // one has to make sure the calculation is done in that state

        G4ThreeVector unrotatedStrawOrigin = sector0RZRot*straw.getMidPoint();

        G4ThreeVector const unrotatedMid(unrotatedStrawOrigin.y() - unrotatedSector0Origin.y(),
                                         unrotatedStrawOrigin.z() - unrotatedSector0Origin.z(),
                                         unrotatedStrawOrigin.x() - unrotatedSector0Origin.x());

        G4ThreeVector const zeroVector(0.0,0.0,0.0);

        if ( verbosityLevel > 2 ) {

          G4ThreeVector const mid(straw.getMidPoint().y() - sector0.boxOffset().y(),
                                  straw.getMidPoint().z() - sector0.boxOffset().z(),
                                  straw.getMidPoint().x() - sector0.boxOffset().x());

          cout << __func__ << " istr: " << istr <<
            " mid: " << mid <<
            ", unrotated mid: " << unrotatedMid <<
            ", straw.MidPoint " << straw.getMidPoint() <<
            ", sector.boxOffset " <<  sector0.boxOffset() <<
            ", device.origin " << device0.origin() <<
            endl;

          cout << __func__ << " istr: " << istr << " mid: " <<
            mid << ", halflenght " << detail.halfLength() << endl;

          // look at StrawSD to see how the straw index is reconstructed

          cout << __func__ << " straw.id(), straw.index() " <<
            straw.id() << ", " << straw.index() << endl;

          int oldPrecision = cout.precision(newPrecision);
          int oldWidth = cout.width(newWidth);
          std::ios::fmtflags oldFlags = cout.flags();
          cout.setf(std::ios::fixed,std::ios::floatfield);
          cout << __func__ << " Straw istr, RYForTrapezoids, midpoint: " <<
            istr << ", " << RYForTrapezoids << ", " <<
            mid << ", " <<
            endl;
          cout.setf(oldFlags);
          cout.precision(oldPrecision);
          cout.width(oldWidth);

        }

        // make the straws more distinguishable when displayed
        G4Colour wallColor = (ilay%2 == 1) ?
          ((istr%2 == 0) ? G4Colour::Green() : G4Colour::Yellow()) :
          ((istr%2 == 0) ? G4Colour::Red() : G4Colour::Blue());

        G4Colour gasColor = (ilay%2 == 0) ?
          ((istr%2 == 0) ? G4Colour::Green() : G4Colour::Yellow()) :
          ((istr%2 == 0) ? G4Colour::Red() : G4Colour::Blue());

        G4Colour wireColor = G4Colour::Cyan();

        VolumeInfo strawWallInfo  = nestTubs(straw.name("TTrackerStrawWall_"),
                                             strawWallParams,
                                             findMaterialOrThrow(detail.wallMaterialName() ),
                                             rotTub,
                                             unrotatedMid,
                                             sectorInfo.logical,
                                             straw.index().asInt(),
                                             ttrackerStrawVisible,
                                             wallColor,
                                             ttrackerStrawSolid,
                                             forceAuxEdgeVisible,
                                             true,
                                             doSurfaceCheck
                                             );

        // may be not all straws have to be in the volInfo registry, another param?
        // we use the Straw name facility to make them unique

        VolumeInfo strawGasInfo  = nestTubs(straw.name("TTrackerStrawGas_"),
                                            strawGasParams,
                                            findMaterialOrThrow(detail.gasMaterialName()),
                                            0,
                                            zeroVector,
                                            strawWallInfo.logical,
                                            straw.index().asInt(),
                                            ttrackerStrawVisible,
                                            gasColor,
                                            ttrackerStrawSolid,
                                            forceAuxEdgeVisible,
                                            true,
                                            doSurfaceCheck
                                            );

        VolumeInfo strawWireInfo  = nestTubs(straw.name("TTrackerStrawWire_"),
                                             strawWireParams,
                                             findMaterialOrThrow(detail.wireMaterialName()),
                                             0,
                                             zeroVector,
                                             strawGasInfo.logical,
                                             straw.index().asInt(),
                                             ttrackerStrawVisible,
                                             wireColor,
                                             ttrackerStrawSolid,
                                             forceAuxEdgeVisible,
                                             true,
                                             doSurfaceCheck
                                             );

        // Make gas of this straw a sensitive detector.
        G4VSensitiveDetector *sd = G4SDManager::GetSDMpointer()->
          FindSensitiveDetector(SensitiveDetectorName::TrackerGas());
        if(sd) strawGasInfo.logical->SetSensitiveDetector(sd);

        sd = G4SDManager::GetSDMpointer()->
          FindSensitiveDetector(SensitiveDetectorName::TrackerSWires());
        if(sd) strawWireInfo.logical->SetSensitiveDetector(sd);

        sd = G4SDManager::GetSDMpointer()->
          FindSensitiveDetector(SensitiveDetectorName::TrackerWalls());
        if (sd) strawWallInfo.logical->SetSensitiveDetector(sd);

      }   // end loop over straws
    }     // end loop over layers

    // We have constructed one sector, 

    // Now construct the devices and place the sectors in them

    vector<VolumeInfo>  deviceInfoVect;
    vector<VolumeInfo> supportInfoVect;
    int tndev = ttracker.nDevices();
    deviceInfoVect.reserve(tndev);
    supportInfoVect.reserve(tndev);

    // idev can't be size_t here as deviceDraw can be -1
    for ( int idev=0; idev<tndev; ++idev ){

      if ( deviceDraw > -1 && idev != deviceDraw )  continue;

      if (verbosityLevel > 0 ) {
        cout << __func__ << " working on device:   " << idev << endl;
      }

      const Device& device = ttracker.getDevice(idev);

      if (!device.exists()) continue;
      if (verbosityLevel > 0 ) {
	cout << __func__ << " existing   device:   " << idev << endl;
      }

      std::ostringstream devs;
      devs << idev;

      CLHEP::HepRotationZ deviceRZ(-device.rotation()); //It is arround z
      G4RotationMatrix* deviceRotation  = reg.add(G4RotationMatrix(deviceRZ));

      // device.origin() is in detector coordinates.
      // devOrigin is in the coordinate system of the mother volume.
      CLHEP::Hep3Vector devOrigin = device.origin() + originOffset;

      deviceInfoVect.push_back(nestTubs("TTrackerDeviceEnvelope_"  + devs.str(),
                                        deviceEnvelopeParams,
                                        envelopeMaterial,
                                        deviceRotation,
                                        devOrigin,
                                        motherInfo.logical,
                                        idev,
                                        ttrackerDeviceEnvelopeVisible,
                                        G4Colour::Magenta(),
                                        ttrackerDeviceEnvelopeSolid,
                                        forceAuxEdgeVisible,
                                        true,
                                        doSurfaceCheck
                                        ));
      
      verbosityLevel > 1 &&
        cout << __func__ << " placing device: " << idev << " " << devOrigin << " " 
             << deviceInfoVect[idev].name << endl;

      // placing support

      TubsParams ttrackerDeviceSupportParams = ttracker.getSupportParams().getTubsParams();

      G4Colour  lightBlue (0.0, 0.0, 0.75);
      supportInfoVect.push_back(nestTubs("TTrackerDeviceSupport_" + devs.str(),
                                         ttrackerDeviceSupportParams,
                                         findMaterialOrThrow(ttracker.getSupportParams().materialName()),
                                         0,
                                         zeroVector,
                                         deviceInfoVect[idev].logical,
                                         idev,
                                         ttrackerSupportVisible,
                                         lightBlue,
                                         ttrackerSupportSolid,
                                         forceAuxEdgeVisible,
                                         true,
                                         doSurfaceCheck
                                         ));
    
      if ( verbosityLevel > 0 && idev==0) {
        int oldPrecision = cout.precision(newPrecision);
        int oldWidth = cout.width(newWidth);
        std::ios::fmtflags oldFlags = cout.flags();
        cout.setf(std::ios::fixed,std::ios::floatfield);

        cout << __func__ << " TTrackerDeviceSupport params: "
             << ttrackerDeviceSupportParams.innerRadius() << " "
             << ttrackerDeviceSupportParams.outerRadius() << " "
             << ttrackerDeviceSupportParams.zHalfLength() << " "
             << endl;

        cout << __func__ << " device env idev, deviceEnvelopeParams ir,or,zhl,phi0,phimax: " <<
          idev << ", " <<
          deviceEnvelopeParams.innerRadius() << ", " <<
          deviceEnvelopeParams.outerRadius() << ", " <<
          deviceEnvelopeParams.zHalfLength() << ", " <<
          deviceEnvelopeParams.phi0()        << ", " <<
          deviceEnvelopeParams.phiMax()      << ", " <<
          endl;
        cout.setf(oldFlags);
        cout.precision(oldPrecision);
        cout.width(oldWidth);
      }

      // Make TTrackerDeviceSupport a sensitive detector for radiation damage studies

      G4VSensitiveDetector *sd = G4SDManager::GetSDMpointer()->
        FindSensitiveDetector(SensitiveDetectorName::TTrackerDeviceSupport());
      if(sd) supportInfoVect[idev].logical->SetSensitiveDetector(sd);

      verbosityLevel > 1 &&
        cout << __func__ << " device: " << idev << " " 
             << deviceInfoVect[idev].name << " deviceDraw: " << deviceDraw << endl;

      if ( verbosityLevel > 1 ) {
        cout << __func__ << " -device.rotation(): " << -device.rotation() << " " << endl;
        cout << __func__ << " device.origin(): " << device.origin() << " " << endl;
      }

      // isec can't be size_t here as sectorDraw can be -1
      for ( int isec = 0; isec<device.nSectors(); ++isec){

        if ( sectorDraw > -1 && isec > sectorDraw ) continue;

        verbosityLevel > 1 &&
          cout << __func__ << " sector: " << isec << " " 
               << sectorInfo.name << " sectorDraw: " << sectorDraw << endl;

        const Sector& sector = device.getSector(isec);

        // place the trapezoid in its position ready for the RZ rotation

        CLHEP::HepRotationZ sectorRZ(sector.boxRzAngle() - device.rotation()); // we know it is only around z...
        // this is a relative rotation and this is what we need to calculate relative positions

        // it is probably the safest to recalculate offsets from the
        // nominal horizontal position and rotations and ignore absolute
        // positions provided by the geometry service

        verbosityLevel > 1 &&
          cout << __func__ << " sector.boxRzAngle(), device.rotation(), diff:   " 
               << sector.boxRzAngle()/M_PI*180. << ", "
               << device.rotation()/M_PI*180.   << ", " 
               << ((sector.boxRzAngle() - device.rotation())/M_PI)*180. << endl;


        // origin a.k.a offset wrt current mother volume
        CLHEP::Hep3Vector sectorOrigin = sector.boxOffset() - device.origin();
        double secRelZ = sectorOrigin.z();

        CLHEP::Hep3Vector nominalRelPos(CLHEP::Hep3Vector(sectorOrigin.x(),sectorOrigin.y(),0.).mag(), 
                                        0., secRelZ);

        if (verbosityLevel > 1) {
          cout << __func__ << " device, sector, isec%2, secRelZ : " 
               << setw(3)  << idev << ", " 
               << isec     << ", " 
               << isec%2   << ", " 
               << setw(10) << secRelZ;
          cout  << endl;
        }

        // we add a 180deg rotation for sector on "even/upstream" side of devices
        
        G4RotationMatrix* sectorRotation = (secRelZ>0.0) ?
          reg.add(G4RotationMatrix(RXForTrapezoids*RZForTrapezoids*sectorRZ.inverse())):
          reg.add(G4RotationMatrix(RXForTrapezoids*RZForTrapezoids*RX2ForTrapezoids*sectorRZ.inverse()));


        CLHEP::Hep3Vector sectorRelOrigin = sectorRZ*nominalRelPos; 
        // we still need to do a complemetary rotation

        if ( verbosityLevel > 1 ) {
          cout << __func__ << " device.origin:      " << idev << " " << isec 
               << " " << deviceInfoVect[idev].name << device.origin() << endl;
          cout << __func__ << " sector.origin:      " << idev << " " << isec 
               << " " << sectorInfo.name << sectorOrigin << endl;
          cout << __func__ << " nominalRelPos:      " << idev << " " << isec 
               << " " << sectorInfo.name << nominalRelPos << endl;
          cout << __func__ << " sectorRelOrigin:    " << idev << " " << isec 
               << " " << sectorInfo.name << sectorRelOrigin << endl;
          cout << __func__ << " sector.boxOffset(): " << idev << " " << isec 
               << " " << sectorInfo.name << sector.boxOffset() << endl;
        }

        // we may need to keep those pointers somewhre... (this is only the last one...)

        sectorInfo.physical =  new G4PVPlacement(sectorRotation,
                                                 sectorRelOrigin,
                                                 sectorInfo.logical,
                                                 sectorInfo.name,
                                                 deviceInfoVect[idev].logical,
                                                 false,
                                                 isec,
                                                 false);
        if ( doSurfaceCheck) {
          checkForOverlaps( sectorInfo.physical, config, verbosityLevel>0);
        }

        if (verbosityLevel > 1) {

          cout << __func__ << " placed sector: " << isec << " in device " << idev << " " 
               << sectorInfo.name << endl;

          const Layer&  layer  = sector.getLayer(0);
          int nStrawsPerSector = sector.nLayers()  * layer.nStraws();
          int nStrawsPerDevice = device.nSectors() * nStrawsPerSector;

          cout << __func__ << " first straw number in sector " << fixed << setw(4)
               << isec << " in dev " << fixed << setw(4)
               << idev << " should be: " << fixed << setw(8)
               << nStrawsPerSector * isec + nStrawsPerDevice * idev
               << endl;

        }

      } // end loop over sectors
    
      verbosityLevel > 1 &&
        cout << __func__ << " placed device: " << idev << " " << idev%2 << " " 
             << deviceInfoVect[idev].name << endl;

    } // end loop over devices

    return motherInfo;

  } // end of constructTTrackerv3

} // end namespace mu2e
