//
//  Adapter to make CLHEP::RandFlat look like the cernlib rn48.
//
//  $Id: rm48.cc,v 1.2 2010/05/17 21:47:32 genser Exp $
//  $Author: genser $
//  $Date: 2010/05/17 21:47:32 $
//
//  Original author Rob Kutschke.
//
//  See rm48.hh for details.

#include "Mu2eUtilities/inc/rm48.hh"
#include "CLHEP/Random/RandFlat.h"

// Scope is local to this file so namespace is irrelevant.
static CLHEP::RandFlat* distribution(0);

namespace mu2e {

  void setRm48Distribution( CLHEP::RandFlat& dist){
    distribution = &dist;
  }

}

void rm48_ ( double *v, int *n){
  
  //if (!distribution){
  // throw here.
  //}

  for ( int i=0; i<*n; ++i){
    *(v++) = distribution->fire();
  }

}

