//
// Object to calculate t0
//
// $Id: HelixFit.cc,v 1.12 2014/07/10 14:47:26 brownd Exp $
// $Author: brownd $
// $Date: 2014/07/10 14:47:26 $
//
// the following has to come before other BaBar includes
#include "BTrk/BaBar/BaBar.hh"
#include "TrkReco/inc/TrkTimeCalculator.hh"
//CLHEP
#include "CLHEP/Units/PhysicalConstants.h"
// boost
#include <boost/accumulators/accumulators.hpp>
#include "boost_fix/accumulators/statistics/stats.hpp"
#include "boost_fix/accumulators/statistics.hpp"
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
// root
#include "TH1F.h"
// C++
#include <vector>
#include <string>
#include <math.h>
#include <cmath>
using CLHEP::Hep3Vector;
using namespace std;
using namespace boost::accumulators;
namespace mu2e
{

  TrkTimeCalculator::TrkTimeCalculator(fhicl::ParameterSet const& pset) :
    _debug(pset.get<int>("debugLevel",0)),
//    _useflag(pset.get<std::vector<std::string>>("UseFlag")),
//    _dontuseflag(pset.get<std::vector<std::string>>("DontUseFlag",vector<string>{"Outlier","Background"})),
    _fdir((TrkFitDirection::FitDirection)(pset.get<int>("fitdirection",TrkFitDirection::downstream))),
    _avgDriftTime(pset.get<double>("AverageDriftTime",24)), 
    _useTOTdrift(pset.get<bool>("UseTOTDrift",true)),
    _shDtDz(pset.get<double>("StrawHitInversVelocity",0.00535)), // ns/mm
    _shBeta(pset.get<double>("StrawHitBeta",1.)),
    _shErr(pset.get<double>("StrawHitTimeErr",9.7)) // ns effective hit time res. without TOT
  {
    _caloT0Offset[0] = pset.get<double>("Disk0TimeOffset",12.4); // nanoseconds
    _caloT0Offset[1] = pset.get<double>("Disk1TimeOffset",15.7); // nanoseconds
    _caloT0Err[0] = pset.get<double>("Disk0TimeErr",0.8); // nanoseconds
    _caloT0Err[1] = pset.get<double>("Disk1TimeErr",1.7); // nanoseconds
  }

  TrkTimeCalculator::~TrkTimeCalculator() {}

  void TrkTimeCalculator::updateT0(TimeCluster& tc, StrawHitCollection const& shcol){
// FIXME!
  }
  void TrkTimeCalculator::updateT0(HelixSeed& hs, StrawHitCollection const& shcol) {
// FIXME!
  }

  double TrkTimeCalculator::timeOfFlightTimeOffset(double hitz) const {
    return hitz*_shDtDz*_fdir.dzdt();
  }

  double TrkTimeCalculator::caloClusterTimeOffset(int diskId) const {
    double retval(0.0);
    if(diskId > -1 && diskId < 2)
      retval = _caloT0Offset[diskId]*_fdir.dzdt();
    return retval;
  }

  double TrkTimeCalculator::caloClusterTimeErr(int diskId) const {
    double retval(1e10);
    if(diskId > -1 && diskId < 2)
      retval = _caloT0Err[diskId];
    return retval;
  }

  double TrkTimeCalculator::strawHitTime(StrawHit const& sh, StrawHitPosition const& shp) {
    return sh.time() - timeOfFlightTimeOffset(shp.pos().z()) - _avgDriftTime;
  }

  double TrkTimeCalculator::comboHitTime(ComboHit const& ch) {
    if (_useTOTdrift)
      return ch.correctedTime() - timeOfFlightTimeOffset(ch.pos().z());
    else
      return ch.time() - timeOfFlightTimeOffset(ch.pos().z()) - _avgDriftTime;
  }

  double TrkTimeCalculator::caloClusterTime(CaloCluster const& cc) const {
    return cc.time() - caloClusterTimeOffset(cc.diskId());
  }

}
