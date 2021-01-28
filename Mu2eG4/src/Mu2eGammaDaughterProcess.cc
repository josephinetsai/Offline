// Special process to kill events with low energy photon daughters
//
// Original author M. MacKenzie

// Mu2e includes
#include "Mu2eG4/inc/Mu2eGammaDaughterProcess.hh"
#include "Mu2eG4/inc/Mu2eG4UserHelpers.hh"

// G4 includes
#include "G4ios.hh"
#include "G4VParticleChange.hh"
#include "G4Track.hh"
#include "G4Step.hh"

namespace mu2e{

  Mu2eGammaDaughterProcess::Mu2eGammaDaughterProcess(const G4String& aName)
    : G4VDiscreteProcess(aName,fUserDefined)
  {
    verbose_ = 10;
    accepted_ = 0;
    photonEnergy_ = -1.;
    theProcessSubType = 0;
    if (verbose_ > 0) {
      G4cout << GetProcessName() << " is created " << G4endl;
    }
  }

  Mu2eGammaDaughterProcess::~Mu2eGammaDaughterProcess()
  {}

  G4bool Mu2eGammaDaughterProcess::IsApplicable(const G4ParticleDefinition& particle) {
    bool retval = (particle.GetParticleName() == "gamma" ||
                   particle.GetParticleName() == "e+" ||
                   particle.GetParticleName() == "e-");
    if(verbose_ > 1 && retval) G4cout << "Mu2eGammaDaughterProcess::" << __func__ << ": Adding particle: "
                                      << particle.GetParticleName()
                                      << G4endl;
    return retval;
  }

  G4double Mu2eGammaDaughterProcess::PostStepGetPhysicalInteractionLength( const G4Track& track,
                                                                           G4double,
                                                                           G4ForceCondition* condition) {
    if(verbose_ > 9) G4cout << "Mu2eGammaDaughterProcess::" << __func__ << ": Track seen: ID = "
                            << track.GetTrackID() << " Parent ID = " << track.GetParentID()
                            << " CreationCode = " << Mu2eG4UserHelpers::findCreationCode(&track)
                            << " E = " << track.GetTotalEnergy()
                            << " E_gamma = " << photonEnergy_ << " accepted = " << accepted_
                            << G4endl;

    // if(accepted_ < 0) { //kill event
    //   return 0.0;
    // } else if(accepted_ > 0) { //accepted event
    //   return DBL_MAX;
    // }

    if(track.GetTrackID() == 1) { //generated photon
      double energy = track.GetTotalEnergy();
      //update the photon energy if no energy is stored or not too small of an energy
      if(photonEnergy_ < 0. || energy > 1.) {
        photonEnergy_ = energy;
      }
      if(photonEnergy_ > minDaughterEnergy_) accepted_ = 0;
      if(verbose_ > 1) G4cout << "Mu2eGammaDaughterProcess::" << __func__ << ": Updating photon energy" << G4endl;
      return DBL_MAX;
    } else if(track.GetParentID() == 1) { //daughter of generated photon
      double energy = track.GetTotalEnergy();
      if(energy > minDaughterEnergy_) {
        if(verbose_ > 0) G4cout << "Mu2eGammaDaughterProcess::" << __func__ << ": Event passed test" << G4endl;
        accepted_ = 1;
        return DBL_MAX;
      } else if(photonEnergy_ > 0. && photonEnergy_ - energy < minDaughterEnergy_) {
        accepted_ = -1; //fails
        if(verbose_ > 0) G4cout << "Mu2eGammaDaughterProcess::" << __func__ << ": Event failed test" << G4endl;
        return 0.0;
      }
      if(verbose_ > 1) G4cout << "Mu2eGammaDaughterProcess::" << __func__ << ": Event continues through test" << G4endl;
      accepted_ = 0;
      return DBL_MAX; //still could have produced a passing daughter --> continue
    }
    if(verbose_ > 9) G4cout << "Mu2eGammaDaughterProcess::" << __func__ << ": Event not tested" << G4endl;
    accepted_ = 0;
    return DBL_MAX; //not the primary photon or a daughter of it
  }

  G4double Mu2eGammaDaughterProcess::GetMeanFreePath(const G4Track&,G4double,
                                                     G4ForceCondition*){
    return DBL_MAX;
  }

  G4VParticleChange* Mu2eGammaDaughterProcess::PostStepDoIt(const G4Track& trk,
                                                            const G4Step& step) {
    pParticleChange->Initialize(trk);
    pParticleChange->ProposeTrackStatus(fStopAndKill);
    return pParticleChange;
  }

}
