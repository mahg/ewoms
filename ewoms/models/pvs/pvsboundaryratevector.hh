// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*
  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.

  Consult the COPYING file in the top-level source directory of this
  module for the precise wording of the license and the list of
  copyright holders.
*/
/*!
 * \file
 *
 * \copydoc Ewoms::PvsBoundaryRateVector
 */
#ifndef EWOMS_PVS_BOUNDARY_RATE_VECTOR_HH
#define EWOMS_PVS_BOUNDARY_RATE_VECTOR_HH

#include "pvsproperties.hh"

#include <ewoms/models/common/energymodule.hh>
#include <opm/common/Valgrind.hpp>

namespace Ewoms {

/*!
 * \ingroup PvsModel
 *
 * \brief Implements a rate vector on the boundary for the fully
 *        implicit compositional multi-phase primary variable
 *        switching compositional model.
 */
template <class TypeTag>
class PvsBoundaryRateVector : public GET_PROP_TYPE(TypeTag, RateVector)
{
    typedef typename GET_PROP_TYPE(TypeTag, RateVector) ParentType;
    typedef typename GET_PROP_TYPE(TypeTag, ExtensiveQuantities) ExtensiveQuantities;
    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, Evaluation) Evaluation;
    typedef typename GET_PROP_TYPE(TypeTag, Indices) Indices;

    enum { numEq = GET_PROP_VALUE(TypeTag, NumEq) };
    enum { numPhases = GET_PROP_VALUE(TypeTag, NumPhases) };
    enum { numComponents = GET_PROP_VALUE(TypeTag, NumComponents) };
    enum { conti0EqIdx = Indices::conti0EqIdx };
    enum { enableEnergy = GET_PROP_VALUE(TypeTag, EnableEnergy) };

    typedef Ewoms::EnergyModule<TypeTag, enableEnergy> EnergyModule;
    typedef Opm::MathToolbox<Evaluation> Toolbox;

public:
    PvsBoundaryRateVector()
        : ParentType()
    {}

    /*!
     * \copydoc
     * ImmiscibleBoundaryRateVector::ImmiscibleBoundaryRateVector(Scalar)
     */
    PvsBoundaryRateVector(const Evaluation& value)
        : ParentType(value)
    {}

    /*!
     * \copydoc ImmiscibleBoundaryRateVector::ImmiscibleBoundaryRateVector(const
     * ImmiscibleBoundaryRateVector& )
     */
    PvsBoundaryRateVector(const PvsBoundaryRateVector& value) = default;
    PvsBoundaryRateVector& operator=(const PvsBoundaryRateVector& value) = default;

    /*!
     * \copydoc ImmiscibleBoundaryRateVector::setFreeFlow
     */
    template <class Context, class FluidState>
    void setFreeFlow(const Context& context, unsigned bfIdx, unsigned timeIdx, const FluidState& fluidState)
    {
        typename FluidSystem::template ParameterCache<typename FluidState::Scalar> paramCache;
        paramCache.updateAll(fluidState);

        ExtensiveQuantities extQuants;
        extQuants.updateBoundary(context, bfIdx, timeIdx, fluidState, paramCache);
        const auto& insideIntQuants = context.intensiveQuantities(bfIdx, timeIdx);

        ////////
        // advective fluxes of all components in all phases
        ////////
        (*this) = Evaluation(0.0);
        for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++phaseIdx) {
            Evaluation meanMBoundary = 0;
            for (unsigned compIdx = 0; compIdx < numComponents; ++compIdx)
                meanMBoundary +=
                    fluidState.moleFraction(phaseIdx, compIdx)*FluidSystem::molarMass(compIdx);

            Evaluation density;
            if (fluidState.pressure(phaseIdx) > insideIntQuants.fluidState().pressure(phaseIdx))
                density = FluidSystem::density(fluidState, paramCache, phaseIdx);
            else
                density = insideIntQuants.fluidState().density(phaseIdx);

            for (unsigned compIdx = 0; compIdx < numComponents; ++compIdx) {
                Evaluation molarity;
                if (fluidState.pressure(phaseIdx) > insideIntQuants.fluidState().pressure(phaseIdx))
                    molarity = fluidState.moleFraction(phaseIdx, compIdx)*density/meanMBoundary;
                else
                    molarity = insideIntQuants.fluidState().molarity(phaseIdx, compIdx);

                // add advective flux of current component in current
                // phase
                (*this)[conti0EqIdx + compIdx] += extQuants.volumeFlux(phaseIdx)*molarity;
            }

            if (enableEnergy) {
                Evaluation specificEnthalpy;
                if (fluidState.pressure(phaseIdx) > insideIntQuants.fluidState().pressure(phaseIdx))
                    specificEnthalpy = FluidSystem::enthalpy(fluidState, paramCache, phaseIdx);
                else
                    specificEnthalpy = insideIntQuants.fluidState().enthalpy(phaseIdx);

                // currently we neglect heat conduction!
                Evaluation enthalpyRate = density*extQuants.volumeFlux(phaseIdx)*specificEnthalpy;
                EnergyModule::addToEnthalpyRate(*this, enthalpyRate);
            }
        }

        EnergyModule::addToEnthalpyRate(*this, EnergyModule::heatConductionRate(extQuants));

#ifndef NDEBUG
        for (unsigned i = 0; i < numEq; ++i)
            Opm::Valgrind::CheckDefined((*this)[i]);
#endif
    }

    /*!
     * \copydoc ImmiscibleBoundaryRateVector::setInFlow
     */
    template <class Context, class FluidState>
    void setInFlow(const Context& context,
                   unsigned bfIdx,
                   unsigned timeIdx,
                   const FluidState& fluidState)
    {
        this->setFreeFlow(context, bfIdx, timeIdx, fluidState);

        // we only allow fluxes in the direction opposite to the outer unit normal
        for (unsigned eqIdx = 0; eqIdx < numEq; ++eqIdx) {
            Evaluation& val = this->operator[](eqIdx);
            val = Toolbox::min(0.0, val);
        }
    }

    /*!
     * \copydoc ImmiscibleBoundaryRateVector::setOutFlow
     */
    template <class Context, class FluidState>
    void setOutFlow(const Context& context,
                    unsigned bfIdx,
                    unsigned timeIdx,
                    const FluidState& fluidState)
    {
        this->setFreeFlow(context, bfIdx, timeIdx, fluidState);

        // we only allow fluxes in the same direction as the outer unit normal
        for (unsigned eqIdx = 0; eqIdx < numEq; ++eqIdx) {
            Evaluation& val = this->operator[](eqIdx);
            val = Toolbox::max(0.0, val);
        }
    }

    /*!
     * \copydoc ImmiscibleBoundaryRateVector::setNoFlow
     */
    void setNoFlow()
    { (*this) = Evaluation(0.0); }
};

} // namespace Ewoms

#endif
