//
//  Filter for selecting good time cluster: this is part of the track trigger
//  Original author: Dave Brown (LBNL) 3/1/2017
//
// framework
#include "art/Framework/Core/EDFilter.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "fhiclcpp/ParameterSet.h"
// mu2e
// data
#include "RecoDataProducts/inc/TimeCluster.hh"
#include "RecoDataProducts/inc/TriggerInfo.hh"
// c++
#include <iostream>
#include <memory>

namespace mu2e
{
  class TimeClusterFilter : public art::EDFilter
  {
  public:
    explicit TimeClusterFilter(fhicl::ParameterSet const& pset);
    virtual bool filter(art::Event& event) override;
    virtual bool endRun( art::Run& run ) override;

  private:
    art::InputTag _tcTag;
    bool          _hascc; // Calo Cluster
    unsigned      _minnhits;
    double        _mintime, _maxtime;
    int           _debug;
    // counters
    unsigned _nevt, _npass;
  };

  TimeClusterFilter::TimeClusterFilter(fhicl::ParameterSet const& pset) :
    art::EDFilter{pset},
    _tcTag(pset.get<art::InputTag>("timeClusterCollection","TimeClusterFinder")),
    _hascc(pset.get<bool>("requireCaloCluster",false)),
    _minnhits(pset.get<unsigned>("minNHits",11)),
    _mintime(pset.get<double>("minTime",500.0)),
    _maxtime(pset.get<double>("maxTime",1695.0)) ,
    _debug(pset.get<int>("debugLevel",0)),
    _nevt(0), _npass(0)
  {
    produces<TriggerInfo>();
  }

  bool TimeClusterFilter::filter(art::Event& evt){
    // create output
    std::unique_ptr<TriggerInfo> triginfo(new TriggerInfo);
    ++_nevt;
    bool retval(false); // preset to fail
    // find the collection
    auto tcH = evt.getValidHandle<TimeClusterCollection>(_tcTag);
    const TimeClusterCollection* tccol = tcH.product();
    // loop over the collection: if any pass the selection, pass this event
    for(auto itc = tccol->begin();itc != tccol->end(); ++itc) {
      auto const& tc = *itc;
      if(_debug > 2){
        std::cout << moduleDescription().moduleLabel() << " nhits = " << tc.hits().size() << " t0 = " << tc.t0().t0() << std::endl;
      }
      if( (!_hascc || tc.caloCluster().isNonnull()) &&
          tc.hits().size() >= _minnhits &&
          tc.t0().t0() > _mintime && tc.t0().t0() < _maxtime) {
        retval = true;
        ++_npass;
        // Fill the trigger info object
        // associate to the hit cluster which triggers.  Note there may be other hit clusters which also pass the filter
        // but filtering is by event!
        size_t index = std::distance(tccol->begin(),itc);
	triginfo->_hitClusters.push_back(art::Ptr<TimeCluster>(tcH,index));
        if(_debug > 1){
          std::cout << moduleDescription().moduleLabel() << " passed event " << evt.id() << std::endl;
        }
      }
    }
    evt.put(std::move(triginfo));
    return retval;
  }

  bool TimeClusterFilter::endRun( art::Run& run ) {
    if(_debug > 0 && _nevt > 0){
      std::cout << moduleDescription().moduleLabel() << " passed " << _npass << " events out of " << _nevt << " for a ratio of " << float(_npass)/float(_nevt) << std::endl;
    }
    return true;
  }
}
using mu2e::TimeClusterFilter;
DEFINE_ART_MODULE(TimeClusterFilter);
