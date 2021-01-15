#ifndef Mu2eG4_Mu2eG4SteppingAction_hh
#define Mu2eG4_Mu2eG4SteppingAction_hh
//
// Called at every G4 step.
//
// Original author Rob Kutschke
//
#include <vector>
#include <string>

// Mu2e includes
#include "Mu2eG4/inc/EventNumberList.hh"
#include "MCDataProducts/inc/ProcessCode.hh"
#include "MCDataProducts/inc/StepPointMCCollection.hh"
#include "MCDataProducts/inc/MCTrajectoryPoint.hh"
#include "Mu2eG4/inc/IMu2eG4Cut.hh"
#include "Mu2eG4/inc/Mu2eG4Config.hh"

// G4 includes
#include "CLHEP/Vector/ThreeVector.h"
#include "G4UserSteppingAction.hh"
#include "G4TrackStatus.hh"
#include "G4ThreeVector.hh"

// Forward declarations outside of mu2e namespace.
class G4VPhysicalVolume;
class G4Track;

namespace mu2e {

  // Forward declarations in mu2e namespace
  class SimParticleHelper;
  class PhysicsProcessInfo;
  class Mu2eG4ResourceLimits;
  class Mu2eG4TrajectoryControl;

  class Mu2eG4SteppingAction : public G4UserSteppingAction
  {

  public:
    Mu2eG4SteppingAction(const Mu2eG4Config::Debug& debug,
			 const Mu2eG4Config::Physics& physics,
                         const std::vector<double>& timeVDtimes,
                         IMu2eG4Cut& steppingCuts,
                         IMu2eG4Cut& commonCuts,
                         const Mu2eG4TrajectoryControl& tc,
                         const Mu2eG4ResourceLimits& mu2elimits);

    void UserSteppingAction(const G4Step*);

    void BeginOfEvent(StepPointMCCollection& outputHits, const SimParticleHelper& spHelper);

    void BeginOfTrack();
    void EndOfTrack();

    int nKilledStepLimit() const { return numKilledTracks_; }

    // Called by G4_plugin.
    void beginRun(PhysicsProcessInfo*, CLHEP::Hep3Vector const& mu2eOrigin );

    // Called by G4_plugin: the final phase of the c'tor cannot be completed until after
    // G4 has initialized itself.
    void finishConstruction();

    std::vector<MCTrajectoryPoint> const&  trajectory();

    // Give away ownership of the trajectory information ( to the data product ).
    // This is called from TrackingAction::addTrajectory which is called from
    // TrackingAction::PostUserTrackingAction.  The result is that the
    // _trajectory data member is empty.
    void swapTrajectory( std::vector<MCTrajectoryPoint>& trajectory);

    // A helper function to manage the printout.
    static void printit( G4String const& s,
                         G4int id,
                         G4ThreeVector const& pos,
                         G4ThreeVector const& mom,
                         double localTime,
                         double globalTime );

  private:
    // owned by Mu2eG4 module.
    IMu2eG4Cut* steppingCuts_;
    IMu2eG4Cut* commonCuts_;

    const Mu2eG4ResourceLimits *mu2elimits_;

    // Protection against "too complicated" events
    unsigned numTrackSteps_;
    int numKilledTracks_;
    bool stepLimitKillerVerbose_;

    // List of times for time virtual detector
    std::vector<double> tvd_time_;
    StepPointMCCollection* tvd_collection_;
    bool tvd_warning_printed_;

    // MCTrajectory point filtering cuts
    const Mu2eG4TrajectoryControl* trajectoryControl_;
    typedef std::map<const G4VPhysicalVolume*, double> VolumeCutMap;
    VolumeCutMap mcTrajectoryVolumePtDistances_;
    // Store trajectory parameters at each G4Step; cleared at beginOfTrack time.
    std::vector<MCTrajectoryPoint> _trajectory;

    // Values to kill low momentum tracks from photon conversions/Compton scatters
    //minimum energy a daughter must have, <= 0 to not kill tracks
    double gammaFilterMinDaughterEnergy_;
    //maximum endpoint value intended with dataset, assumed partner of a given track could have
    // this energy when deciding whether or not to kill the track if haven't found energy  yet
    double gammaFilterMaxGammaEnergy_;
    //kill event once the photon converts, useful if only interested in conversion points
    bool   gammaFilterKillAfterConvert_;
    //photon's energy in current event 
    double gammaFilterPhotonEnergy_;
    int    gammaFilterAccepted_; // 0 = undetermined, -1 = kill event, 1 = accept event

    // Lists of events and tracks for which to enable debug printout.
    EventNumberList _debugEventList;
    EventNumberList _debugTrackList;

    // Information about the SimParticleCollection, needed to instantiate art::Ptr.
    const SimParticleHelper *_spHelper;

    // Non-owning pointer to the information about physical processes;
    // lifetime of pointee is one run.
    PhysicsProcessInfo *  _processInfo;

    // Origin of Mu2e Coordinate system in the G4 world system.
    CLHEP::Hep3Vector _mu2eOrigin;

    // Functions to decide whether or not to kill tracks.
    bool killTooManySteps ( const G4Track* const);

    // Function to decide whether or not to kill an RMC daughter track, and the event with it.
    bool killLowMomentumGammaDaughters( const G4Track* const track);

    // A helper function to kill the track and record the reason for killing it.
    void killTrack( G4Track* track, ProcessCode::enum_type code, G4TrackStatus status );

    // Add time virtual detector hit to the collection
    G4bool addTimeVDHit(const G4Step*, int);

    // per-volume or the default
    double mcTrajectoryMinDistanceCut(const G4VPhysicalVolume* vol) const;
  };

} // end namespace mu2e
#endif /* Mu2eG4_Mu2eG4SteppingAction_hh */
