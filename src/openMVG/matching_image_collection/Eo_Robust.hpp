// This file is part of OpenMVG, an Open Multiple View Geometry C++ library.

// Copyright (c) 2017 Pierre MOULON, Romain JANVIER.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENMVG_MATCHING_IMAGE_COLLECTION_EO_ROBUST_HPP
#define OPENMVG_MATCHING_IMAGE_COLLECTION_EO_ROBUST_HPP

#include <limits>
#include <utility>
#include <vector>

#include "openMVG/cameras/Camera_Pinhole.hpp"
#include "openMVG/matching/indMatch.hpp"
#include "openMVG/matching_image_collection/Geometric_Filter_utils.hpp"
#include "openMVG/multiview/solver_essential_kernel.hpp"
#include "openMVG/multiview/essential.hpp"
#include "openMVG/robust_estimation/robust_estimator_Ransac.hpp"
#include "openMVG/robust_estimation/score_evaluator.hpp"
#include "openMVG/robust_estimation/guided_matching.hpp"
#include "openMVG/sfm/sfm_data.hpp"
#include "openMVG/types.hpp"

namespace openMVG {

namespace sfm {
struct Regions_Provider;
}

namespace matching_image_collection {

//-- A contrario essential matrix estimation template functor used for filter pair of putative correspondences
struct GeometricFilter_EOMatrix_RA
{
    GeometricFilter_EOMatrix_RA(double dPrecision = std::numeric_limits<double>::infinity())
            : m_dPrecision(dPrecision), m_E(Mat3::Identity()){}

    /// Robust fitting of the ESSENTIAL matrix
    template<typename Regions_or_Features_ProviderT>
    bool Robust_estimation(
            const sfm::SfM_Data * sfm_data,
            const std::shared_ptr<Regions_or_Features_ProviderT> & regions_provider,
            const Pair pairIndex,
            const matching::IndMatches & vec_PutativeMatches,
            matching::IndMatches & geometric_inliers)
    {
      using namespace openMVG;
      using namespace openMVG::robust;
      geometric_inliers.clear();

      // Get back corresponding view index
      const IndexT iIndex = pairIndex.first;
      const IndexT jIndex = pairIndex.second;

      //--
      // Reject pair with missing Intrinsic information
      //--

      const sfm::View * view_I = sfm_data->views.at(iIndex).get();
      const sfm::View * view_J = sfm_data->views.at(jIndex).get();

      // Check that valid cameras can be retrieved for the pair of views
      const cameras::IntrinsicBase * cam_I =
              sfm_data->GetIntrinsics().count(view_I->id_intrinsic) ?
              sfm_data->GetIntrinsics().at(view_I->id_intrinsic).get() : nullptr;
      const cameras::IntrinsicBase * cam_J =
              sfm_data->GetIntrinsics().count(view_J->id_intrinsic) ?
              sfm_data->GetIntrinsics().at(view_J->id_intrinsic).get() : nullptr;

      if (!cam_I || !cam_J)
        return false;
      if ( !isPinhole(cam_I->getType()) || !isPinhole(cam_J->getType()))
        return false;

      //--
      // Get corresponding point regions arrays
      //--

      Mat xI,xJ;
      MatchesPairToMat(pairIndex, vec_PutativeMatches, sfm_data, regions_provider, xI, xJ);

      //--
      // Robust estimation
      //--

      // Define the Kernel
      // --- using KernelType = essential::kernel::ThreePointKernel;
      using KernelType =
      ACKernelAdaptorEssentialOrtho<
              essential::kernel::ThreePointKernel,
              essential::kernel::OrthographicSymetricEpipolarDistanceError,
              Mat3>;

      const cameras::Pinhole_Intrinsic * ptrPinhole_I = dynamic_cast<const cameras::Pinhole_Intrinsic*>(cam_I);
      const cameras::Pinhole_Intrinsic * ptrPinhole_J = dynamic_cast<const cameras::Pinhole_Intrinsic*>(cam_J);

      // --- KernelType kernel(xI, xJ, ptrPinhole_I->K(), ptrPinhole_J->K());

      const KernelType kernel(
              xI, sfm_data->GetViews().at(iIndex)->ui_width, sfm_data->GetViews().at(iIndex)->ui_height,
              xJ, sfm_data->GetViews().at(jIndex)->ui_width, sfm_data->GetViews().at(jIndex)->ui_height,
              ptrPinhole_I->K(), ptrPinhole_J->K());

      // Robustly estimate the Essential matrix with RANSAC
      std::vector<uint32_t> vec_inliers;
      size_t inlier_count = 0;

      // -- m_E = RANSAC(kernel, ScorerEvaluator<KernelType>(m_dPrecision), &vec_inliers, &inlier_count);
      const std::pair<double,double> ACRansacOut =
              ACRANSAC(kernel, vec_inliers, 4096, &m_E, m_dPrecision);


      if (vec_inliers.size() > KernelType::MINIMUM_SAMPLES * 2.5)  {
        // update geometric_inliers
        geometric_inliers.reserve(vec_inliers.size());
        for (const uint32_t & index : vec_inliers) {
          geometric_inliers.push_back( vec_PutativeMatches[index] );
        }
        return true;
      }
      else  {
        vec_inliers.clear();
        return false;
      }
    }

    bool Geometry_guided_matching
            (
                    const sfm::SfM_Data * sfm_data,
                    const std::shared_ptr<sfm::Regions_Provider> & regions_provider,
                    const Pair pairIndex,
                    const double dDistanceRatio,
                    matching::IndMatches & matches
            )
    {
      return false;
    }

    double m_dPrecision;  //upper_bound precision used for robust estimation
    //
    //-- Stored data
    Mat3 m_E;
};

} //namespace matching_image_collection
}  // namespace openMVG

#endif //OPENMVG_MATCHING_IMAGE_COLLECTION_EO_ROBUST_HPP
