#include "sbnana/CAFAna/Core/Tree.h"

#include "TDirectory.h"
#include "TBranch.h"
#include "TTree.h"

#include <cassert>

namespace ana
{

  //----------------------------------------------------------------------
  // Constructor for a set of Vars, typical usage for a Selected Tree
  Tree::Tree( const std::string name, const std::vector<std::string>& labels,
              SpectrumLoaderBase& loader,
              const std::vector<Var>& vars, const SpillCut& spillcut,
              const Cut& cut, const SystShifts& shift )
    : fTreeName(name), fNEntries(0), fPOT(0), fLivetime(0)
  {
    assert( labels.size() == vars.size() );

    for ( unsigned int i=0; i<labels.size(); ++i ) {
      fOrderedBranchNames.push_back( labels.at(i) );
      fBranchEntries[labels.at(i)] = {};
    }

    loader.AddTree( *this, labels, vars, spillcut, cut, shift );
  }

  //----------------------------------------------------------------------
  // Constructor for a set of MultiVars
  Tree::Tree( const std::string name, const std::vector<std::string>& labels,
              SpectrumLoaderBase& loader,
              const std::vector<MultiVar>& vars, const SpillCut& spillcut,
              const Cut& cut, const SystShifts& shift )
    : fTreeName(name), fNEntries(0), fPOT(0), fLivetime(0)
  {
    assert( labels.size() == vars.size() );

    for ( unsigned int i=0; i<labels.size(); ++i ) {
      fOrderedBranchNames.push_back( labels.at(i) );
      fBranchEntries[labels.at(i)] = {};
    }

    loader.AddTree( *this, labels, vars, spillcut, cut, shift );
  }

  //----------------------------------------------------------------------
  // Constructor for a set of SpillVars
  Tree::Tree( const std::string name, const std::vector<std::string>& labels,
              SpectrumLoaderBase& loader,
              const std::vector<Var>& vars, const SpillCut& spillcut)
    : fTreeName(name), fNEntries(0), fPOT(0), fLivetime(0)
  {
    assert( labels.size() == vars.size() );

    for ( unsigned int i=0; i<labels.size(); ++i ) {
      fOrderedBranchNames.push_back( labels.at(i) );
      fBranchEntries[labels.at(i)] = {};
    }

    loader.AddTree( *this, labels, vars, spillcut, cut, shift );
  }

  //----------------------------------------------------------------------
  // Add an entry to a branch
  void Tree::AddEntry( const std::string name, const double val )
  {
    assert ( fBranchEntries.find(name) != fBranchEntries.end() );
  
    fBranchEntries.at(name).push_back(val);
  }

  //----------------------------------------------------------------------
  // Save to a ROOT file directory as a TTree -- similar to Spectrum::SaveTo()
  void Tree::SaveTo( TDirectory* dir ) const
  {
    std::cout << "WRITING A TTree FOR THIS Tree OBJECT WITH:" << std::endl;
    std::cout << "  " << fNEntries << " Entries" << std::endl;
    std::cout << "  For " << fPOT << " POT and " << fLivetime << " Livetime" << std::endl;
    if ( fNEntries>=1 ) {
      std::cout << "  Sample:" << std::endl;
      for ( auto const& [key, value] : fBranchEntries ) {
        std::cout << "    " << key << " has first value " << value.at(0) << std::endl;
      }
    }
    else {
      std::cout << "    Double checking --> " << fOrderedBranchNames.at(0)
                << " has size " <<  fBranchEntries.at( fOrderedBranchNames.at(0) ).size() << std::endl;
    }

    TDirectory *tmp = gDirectory;
    dir->cd();

    TTree theTree( fTreeName.c_str(), fTreeName.c_str() );

    const int NBranches = fOrderedBranchNames.size();
    double entryVals[ NBranches ];

    for ( unsigned int idxBranch=0; idxBranch<fOrderedBranchNames.size(); ++idxBranch ) {
      theTree.Branch( fOrderedBranchNames.at(idxBranch).c_str(), &entryVals[idxBranch] );
    }

    // Loop over entries
    for ( unsigned int idxEntry=0; idxEntry < fNEntries; ++idxEntry ) {
      // Fill up the vals
      for ( unsigned int idxBranch=0; idxBranch < fOrderedBranchNames.size(); ++idxBranch ) {
        entryVals[idxBranch] = fBranchEntries.at( fOrderedBranchNames.at(idxBranch) ).at(idxEntry);
      }
      theTree.Fill();
    }

    theTree.Write();

    tmp->cd();
  }

}