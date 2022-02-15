#include "sbnana/CAFAna/Core/EnsembleSpectrum.h"

#include "sbnana/CAFAna/Core/EnsembleRatio.h"

#include "sbnana/CAFAna/Core/Utilities.h"

#include "TDirectory.h"
#include "TGraphAsymmErrors.h"
#include "TH1.h"
#include "TObjString.h"
#include "TPad.h"

namespace ana
{
  //----------------------------------------------------------------------
  EnsembleSpectrum::EnsembleSpectrum(ISliceSource& src,
                                     const HistAxis& axis,
                                     const std::vector<SystShifts>& univ_shifts)
    : fNom(src, axis)
  {
    // TODO TODO no way to shift a source yet
    //    fUnivs.reserve(univ_shifts.size());
    //    for(const SystShifts& ss: univ_shifts){
    //      fUnivs.emplace_back(src[ss], axis);
    //    }
  }

  //----------------------------------------------------------------------
  EnsembleSpectrum::EnsembleSpectrum(ISliceSource& src,
                                     const HistAxis& axis,
                                     const std::vector<Weight>& univ_weis)
    : fNom(src, axis)
  {
    fUnivs.reserve(univ_weis.size());
    for(const Weight& w: univ_weis){
      fUnivs.emplace_back(src.Weighted(w), axis);
    }
  }

  //----------------------------------------------------------------------
  TGraphAsymmErrors* EnsembleSpectrum::ErrorBand(double exposure,
                                                 EExposureType expotype,
                                                 EBinType bintype) const
  {
    std::unique_ptr<TH1D> hnom(fNom.ToTH1(exposure, expotype, bintype));

    std::vector<std::unique_ptr<TH1D>> hunivs;
    hunivs.reserve(fUnivs.size());
    for(const Spectrum& u: fUnivs) hunivs.emplace_back(u.ToTH1(exposure, expotype, bintype));

    TGraphAsymmErrors* g = new TGraphAsymmErrors;

    for(int binIdx = 0; binIdx < hnom->GetNbinsX()+2; ++binIdx){
      const double xnom = hnom->GetXaxis()->GetBinCenter(binIdx);
      const double ynom = hnom->GetBinContent(binIdx);
      g->SetPoint(binIdx, xnom, ynom);

      const double dx = hnom->GetXaxis()->GetBinWidth(binIdx);

      std::vector<double> ys;
      ys.reserve(hunivs.size());
      for(const std::unique_ptr<TH1D>& hu: hunivs){
        ys.push_back(hu->GetBinContent(binIdx));
      }

      // 1 sigma
      const double y0 = FindQuantile(.5-0.6827/2, ys);
      const double y1 = FindQuantile(.5+0.6827/2, ys);

      // It's theoretically possible for the central value to be outside the
      // error bands - clamp to zero in that case
      g->SetPointError(binIdx, dx/2, dx/2,
                       std::max(ynom-y0, 0.),
                       std::max(y1-ynom, 0.));
    } // end for binIdx

    return g;
  }

  //----------------------------------------------------------------------
  void EnsembleSpectrum::Scale(double c)
  {
    fNom.Scale(c);
    for(Spectrum& s: fUnivs) s.Scale(c);
  }

  //----------------------------------------------------------------------
  EnsembleSpectrum& EnsembleSpectrum::operator+=(const EnsembleSpectrum& rhs)
  {
    fNom += rhs.fNom;
    assert(fUnivs.size() == rhs.fUnivs.size());
    for(unsigned int i = 0; i < fUnivs.size(); ++i) fUnivs[i] += rhs.fUnivs[i];
    return *this;
  }

  //----------------------------------------------------------------------
  EnsembleSpectrum EnsembleSpectrum::operator+(const EnsembleSpectrum& rhs) const
  {
    EnsembleSpectrum ret = *this;
    ret += rhs;
    return ret;
  }

  //----------------------------------------------------------------------
  EnsembleSpectrum& EnsembleSpectrum::operator-=(const EnsembleSpectrum& rhs)
  {
    fNom -= rhs.fNom;
    assert(fUnivs.size() == rhs.fUnivs.size());
    for(unsigned int i = 0; i < fUnivs.size(); ++i) fUnivs[i] -= rhs.fUnivs[i];
    return *this;
  }

  //----------------------------------------------------------------------
  EnsembleSpectrum EnsembleSpectrum::operator-(const EnsembleSpectrum& rhs) const
  {
    EnsembleSpectrum ret = *this;
    ret -= rhs;
    return ret;
  }

  //----------------------------------------------------------------------
  EnsembleSpectrum& EnsembleSpectrum::operator*=(const EnsembleRatio& rhs)
  {
    fNom *= rhs.Nominal();
    assert(rhs.NUniverses() == fUnivs.size());
    for(unsigned int i = 0; i < fUnivs.size(); ++i) fUnivs[i] *= rhs.Universe(i);
    return *this;
  }

  //----------------------------------------------------------------------
  EnsembleSpectrum EnsembleSpectrum::operator*(const EnsembleRatio& rhs) const
  {
    EnsembleSpectrum ret = *this;
    ret *= rhs;
    return ret;
  }

  //----------------------------------------------------------------------
  EnsembleSpectrum& EnsembleSpectrum::operator/=(const EnsembleRatio& rhs)
  {
    fNom /= rhs.Nominal();
    assert(rhs.NUniverses() == fUnivs.size());
    for(unsigned int i = 0; i < fUnivs.size(); ++i) fUnivs[i] /= rhs.Universe(i);
    return *this;
  }

  //----------------------------------------------------------------------
  EnsembleSpectrum EnsembleSpectrum::operator/(const EnsembleRatio& rhs) const
  {
    EnsembleSpectrum ret = *this;
    ret /= rhs;
    return ret;
  }

  //----------------------------------------------------------------------
  void EnsembleSpectrum::SaveTo(TDirectory* dir, const std::string& name) const
  {
    TDirectory* tmp = gDirectory;

    dir = dir->mkdir(name.c_str()); // switch to subdir
    dir->cd();

    TObjString("EnsembleSpectrum").Write("type");

    fNom.SaveTo(dir, "nom");
    for(unsigned int i = 0; i < fUnivs.size(); ++i){
      fUnivs[i].SaveTo(dir, ("univ"+std::to_string(i)).c_str());
    }

    dir->Write();
    delete dir;

    tmp->cd();
  }

  //----------------------------------------------------------------------
  std::unique_ptr<EnsembleSpectrum> EnsembleSpectrum::LoadFrom(TDirectory* dir, const std::string& name)
  {
    dir = dir->GetDirectory(name.c_str()); // switch to subdir
    assert(dir);

    DontAddDirectory guard;

    TObjString* tag = (TObjString*)dir->Get("type");
    assert(tag);
    assert(tag->GetString() == "EnsembleSpectrum");
    delete tag;

    std::unique_ptr<EnsembleSpectrum> ret(new EnsembleSpectrum(*Spectrum::LoadFrom(dir, "nom")));

    for(unsigned int i = 0; ; ++i){
      const std::string uname = "univ"+std::to_string(i);
      TDirectory* d = dir->GetDirectory(uname.c_str());
      if(!d) break;
      ret->fUnivs.push_back(*Spectrum::LoadFrom(dir, uname));
    }

    delete dir;

    return ret;
  }

  //----------------------------------------------------------------------
  void DrawErrorBand(TH1* nom, TGraphAsymmErrors* band, int bandCol, double alpha)
  {
    if(bandCol == -1) bandCol = nom->GetLineColor()-10; // hopefully a lighter version

    // Check if this pad has already been drawn in
    const bool same = gPad && !gPad->GetListOfPrimitives()->IsEmpty();

    nom->Draw(same ? "hist same" : "hist");

    band->SetFillColorAlpha(bandCol, alpha);
    band->Draw("e2 same");

    nom->Draw("hist same");

    // If we are the first spectrum to draw, scale the y-axis appropriately to
    // fit the error band as well as the nominal
    if(!same){
      double maxY = 0;
      // Don't consider underflow or overflow bins when determining maximum
      for(int i = 1; i < band->GetN()-1; ++i){
        maxY = std::max(maxY, band->GetY()[i] + band->GetErrorYhigh(i));
      }

      // Use non-zero lower value so that SetLogy() still works
      nom->GetYaxis()->SetRangeUser(1e-10, 1.1 * maxY);
    }

    gPad->RedrawAxis();
  }
}
