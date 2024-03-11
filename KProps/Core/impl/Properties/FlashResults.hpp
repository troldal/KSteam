//
// Created by kenne on 08/03/2024.
//

#pragma once

#include "../Interfaces/IFluid.hpp"

#include "PropertyProxy.hpp"

namespace pcprops
{

    class FlashResults
    {
        IFluid m_fluid;

    public:
        template<IsProperty P1, IsProperty P2>
        FlashResults(IFluid fluid, P1 p1, P2 p2) : m_fluid(std::move(fluid))
        {
            m_fluid.setState(p1, p2);
        }

        template<typename PROPERTY_T, typename UNITS_T = MolarUnits>
        PROPERTY_T property() const
        {
            return m_fluid.property<PROPERTY_T, UNITS_T>();
        }

        template<typename UNITS_T = MolarUnits>
        auto property(Property prop) const
        {
            return m_fluid.property<UNITS_T>(prop);
        }

        template<typename UNITS_T = MolarUnits>
        auto property(const std::string& propString) const
        {
            return m_fluid.property<UNITS_T>(propString);
        }

        template<typename... PROPERTIES_T>
            requires(sizeof...(PROPERTIES_T) >= 1)
        auto properties()
        {
            return StaticProperties<PROPERTIES_T...>(m_fluid);
        }

        auto properties(std::initializer_list<Property> proplist)
        {
            return DynamicProperties(m_fluid, std::vector(proplist.begin(), proplist.end()));
        }

        auto properties(std::initializer_list<std::string> proplist)
        {
            return DynamicProperties(m_fluid, std::vector(proplist.begin(), proplist.end()));
        }

    };

}    // namespace pcprops