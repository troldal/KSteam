/*
KKKKKKKKK    KKKKKKK   SSSSSSSSSSSSSSS      tttt
K:::::::K    K:::::K SS:::::::::::::::S  ttt:::t
K:::::::K    K:::::KS:::::SSSSSS::::::S  t:::::t
K:::::::K   K::::::KS:::::S     SSSSSSS  t:::::t
KK::::::K  K:::::KKKS:::::S        ttttttt:::::ttttttt        eeeeeeeeeeee    aaaaaaaaaaaaa      mmmmmmm    mmmmmmm
  K:::::K K:::::K   S:::::S        t:::::::::::::::::t      ee::::::::::::ee  a::::::::::::a   mm:::::::m  m:::::::mm
  K::::::K:::::K     S::::SSSS     t:::::::::::::::::t     e::::::eeeee:::::eeaaaaaaaaa:::::a m::::::::::mm::::::::::m
  K:::::::::::K       SS::::::SSSSStttttt:::::::tttttt    e::::::e     e:::::e         a::::a m::::::::::::::::::::::m
  K:::::::::::K         SSS::::::::SS    t:::::t          e:::::::eeeee::::::e  aaaaaaa:::::a m:::::mmm::::::mmm:::::m
  K::::::K:::::K           SSSSSS::::S   t:::::t          e:::::::::::::::::e aa::::::::::::a m::::m   m::::m   m::::m
  K:::::K K:::::K               S:::::S  t:::::t          e::::::eeeeeeeeeee a::::aaaa::::::a m::::m   m::::m   m::::m
KK::::::K  K:::::KKK            S:::::S  t:::::t    tttttte:::::::e         a::::a    a:::::a m::::m   m::::m   m::::m
K:::::::K   K::::::KSSSSSSS     S:::::S  t::::::tttt:::::te::::::::e        a::::a    a:::::a m::::m   m::::m   m::::m
K:::::::K    K:::::KS::::::SSSSSS:::::S  tt::::::::::::::t e::::::::eeeeeeeea:::::aaaa::::::a m::::m   m::::m   m::::m
K:::::::K    K:::::KS:::::::::::::::SS     tt:::::::::::tt  ee:::::::::::::e a::::::::::aa:::am::::m   m::::m   m::::m
KKKKKKKKK    KKKKKKK SSSSSSSSSSSSSSS         ttttttttttt      eeeeeeeeeeeeee  aaaaaaaaaa  aaaammmmmm   mmmmmm   mmmmmm

MIT License

Copyright (c) 2023 Kenneth Troldal Balslev

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef KSTEAM_FLASHTSPEC_HPP
#define KSTEAM_FLASHTSPEC_HPP

#include "Common.hpp"
#include "FlashPSpec.hpp"

#include <numerixx.hpp>

#include <algorithm>
#include <array>

namespace KSteam
{
    /**
     * @brief Calculates the specified thermodynamic property of water/steam at given temperature and quality using the IAPWS-IF97 model.
     * @param temperature The temperature in K.
     * @param quality The steam quality (mass fraction of vapor phase).
     * @param property The thermodynamic property to be calculated (as PropertyType enumeration).
     * @return The calculated property value.
     * @throws XLSteamError If input values are out of range.
     */
    inline FLOAT calcPropertyTX(FLOAT temperature, FLOAT quality, Property property)
    {
        if (temperature < 273.16 || temperature > IF97::get_Tcrit())
            throw KSteamError("Temperature out of range", "calcPropertyTX", { { "T", temperature }, { "x", quality } });
        if (quality < 0.0 || quality > 1.0)
            throw KSteamError("Quality out of range", "calcPropertyTX", { { "T", temperature }, { "x", quality } });

        return calcPropertyPX(IF97::psat97(temperature), quality, property);
    }

    namespace impl
    {
        inline bool temperatureIsInRange(FLOAT temperature) { return temperature >= 273.16 || temperature <= 2273.15; }

        // TODO: This function can be removed when a minimization algorithm is implemented.
        template<Property::Type OtherType>
        inline std::optional<FLOAT> inflictionPressure(FLOAT temperature)
        {
            if (temperature < 277.15) {
                if constexpr (OtherType == Property::Entropy)
                    return { -5.41875E4 * temperature * temperature + 2.5066E7 * temperature - 2.78485E9 };

                if constexpr (OtherType == Property::InternalEnergy)
                    return { -2.66288E5 * temperature * temperature + 1.36472e8 * temperature - 1.73692e10 };
            }

            if (temperature >= 520.7 && temperature <= 613.04) {
                if constexpr (OtherType == Property::Enthalpy)
                    return { 4.8578897349E-08 * std::pow(temperature,6)
                             - 1.7515287788E-04 * std::pow(temperature,5)
                             + 2.6219531615E-01 * std::pow(temperature,4)
                             - 2.0863091760E+02 * std::pow(temperature,3)
                             + 9.2365021333E+04 * std::pow(temperature,2)
                             - 2.0225509689E+07 * temperature
                             + 1.4082100314E+09};
            }

            return std::nullopt;
        }

        template<Property::Type OtherType, typename FN>
        inline auto findPressureBounds(
            FN func, FLOAT temperature, FLOAT otherSpec, std::pair<FLOAT, FLOAT> limits, std::optional<FLOAT> guess = std::nullopt)
        {
            using namespace nxx::roots;

            // Check if the guess is within the limits
            if (guess.has_value() && *guess >= limits.first && *guess <= limits.second) {
                using namespace nxx::roots;
                auto bounds = search(BracketExpandOut(func, limits), std::make_pair(*guess - 1.0, *guess + 1.0));
                return *bounds;
            }

            // Otherwise, make an initial guess, and search for the root brackets
            else {
                auto propLower = calcPropertyPT(limits.first, temperature, OtherType);
                auto propUpper = calcPropertyPT(limits.second, temperature, OtherType);
                auto tempEst   = limits.first + (limits.second - limits.first) * (otherSpec - propLower) / (propUpper - propLower);
                auto bounds    = search(nxx::roots::BracketExpandOut(func, limits), std::make_pair(tempEst - 1.0, tempEst + 1.0));
                return *bounds;
            }
        }

        template<Property::Type OtherType>
        inline FLOAT
            calcTSpecSupercritical(FLOAT temperature, FLOAT otherSpec, Property property, std::optional<FLOAT> guess = std::nullopt)
        {
            using namespace nxx::roots;

            // Define the function to solve.
            auto func = [&](FLOAT p) { return calcPropertyPT(p, temperature, OtherType) - otherSpec; };

            // Determine the limits for the temperature for the given pressure
            auto limits = PressureLimits(temperature);

            // Find the bounds for the pressure
            auto bounds = findPressureBounds<OtherType>(func, temperature, otherSpec, limits, guess);

            // Solve the function
            auto pressure = fsolve(Bisection(func), bounds, EPS * EPS);    // TODO: Can the tolerance be improved?
            return calcPropertyPT((pressure.has_value() ? *pressure : pressure.error().value()), temperature, property);
        }

        template<Property::Type OtherType>
        inline FLOAT calcTSpecSaturation(FLOAT temperature, FLOAT otherSpec, Property property)
        {
            using namespace nxx::roots;

            // Define the function to solve.
            auto func = [&](FLOAT x) { return calcPropertyTX(temperature, x, OtherType) - otherSpec; };

            // Solve the function
            auto quality = fsolve(Bisection(func), { 0.0, 1.0 }, EPS);
            return calcPropertyTX(temperature, *quality, property);
        }

        template<Property::Type OtherType>
        inline FLOAT calcTSpecLiquid(FLOAT temperature, FLOAT otherSpec, Property property, std::optional<FLOAT> guess = std::nullopt)
        {
            using namespace nxx::roots;

            // Define the function to solve.
            auto func = [&](FLOAT p) { return calcPropertyPT(p, temperature, OtherType) - otherSpec; };

            auto lower  = IF97::psat97(temperature) + EPS;
            auto upper  = PressureLimits(temperature).second;
            auto limits = std::make_pair(lower, upper);

            auto inflection = inflictionPressure<OtherType>(temperature);
            if (inflection && guess) {
                if (guess.value() <= inflection.value())
                    limits = std::make_pair(lower, inflection.value());
                else
                    limits = std::make_pair(inflection.value(), upper);
            }

            // Find the bounds for the pressure
            auto bounds = findPressureBounds<OtherType>(func, temperature, otherSpec, limits, guess);

            // Solve the function
            auto pressure = fsolve(Bisection(func), bounds, EPS);
            return calcPropertyPT((pressure.has_value() ? *pressure : pressure.error().value()), temperature, property);
        }

        template<Property::Type OtherType>
        inline FLOAT calcTSpecVapor(FLOAT temperature, FLOAT otherSpec, Property property, std::optional<FLOAT> guess = std::nullopt)
        {
            using namespace nxx::roots;

            // Define the function to solve.
            auto func = [&](FLOAT p) { return calcPropertyPT(p, temperature, OtherType) - otherSpec; };

            // Determine the limits for the temperature for the given pressure
            auto limits = std::make_pair(PressureLimits(temperature).first, IF97::psat97(temperature) - EPS);

            // Find the bounds for the pressure
            auto bounds = findPressureBounds<OtherType>(func, temperature, otherSpec, limits, guess);

            // Solve the function
            auto pressure = fsolve(Bisection(func), bounds, EPS);
            return calcPropertyPT((pressure.has_value() ? *pressure : pressure.error().value()), temperature, property);
        }

        template<Property::Type OtherType>
        inline FLOAT calcTSpec(FLOAT temperature, FLOAT otherSpec, Property property, std::optional<FLOAT> guess = std::nullopt)
        {
            // Look up the string representation of the other property
            auto specString = Property(OtherType).asString();
                //std::find_if(PropertyList.begin(), PropertyList.end(), [&](const auto& s) { return s.second == OtherType; })->first;

            // Check if the temperature is in range
            if (!temperatureIsInRange(temperature))
                throw KSteamError("Temperature out of range", "calcPropertyTV", { { "T", temperature }, { specString, otherSpec } });

            // Check if we are in the supercritical region
            if (temperature > IF97::get_Tcrit()) return calcTSpecSupercritical<OtherType>(temperature, otherSpec, property);

            auto limits       = PressureLimits(temperature);
            auto inflPressure = inflictionPressure<OtherType>(temperature);

            // Determine the volume of the saturated liquid and saturated vapor
            auto propMin    = calcPropertyPT(limits.first, temperature, OtherType);
            auto propVapSat = calcPropertyTX(temperature, 1.0, OtherType);
            auto propLiqSat = calcPropertyTX(temperature, 0.0, OtherType);


            std::array<FLOAT, 3> liqVal {propLiqSat,
                                          calcPropertyPT(limits.second, temperature, OtherType),
                                          (inflPressure ? calcPropertyPT(inflPressure.value(), temperature, OtherType) : propLiqSat)};
//            auto propLiqMin = (inflPressure ? calcPropertyPT(inflPressure.value(), temperature, OtherType) : propLiqSat);
//            auto propLiqMax = calcPropertyPT(limits.second, temperature, OtherType);
            auto propLiqMin = *std::min_element(liqVal.begin(), liqVal.end());
            auto propLiqMax = *std::max_element(liqVal.begin(), liqVal.end());

            // TODO: the addition/subtraction of 0.005 is to account for the tolerance of the solver. Can this be improved?
            auto isInRange = [](std::array<FLOAT, 2> range, FLOAT value, FLOAT tol = 0.0) { return value > range.front() - tol && value < range.back() + tol; };

            // Check the limits for the liquid, vapor, and saturation regions
            auto rangeVap = std::array<FLOAT, 2> { propMin, propVapSat };
            auto rangeSat = std::array<FLOAT, 2> { propLiqSat, propVapSat };
            auto rangeLiq = std::array<FLOAT, 2> { propLiqMin, propLiqMax };

            std::sort(rangeVap.begin(), rangeVap.end());
            std::sort(rangeSat.begin(), rangeSat.end());
            std::sort(rangeLiq.begin(), rangeLiq.end());

            if (guess) {

                auto isVapor = [&] {return isInRange(rangeVap, otherSpec) && guess.value() < IF97::psat97(temperature);};
                auto isLiquid = [&] {return isInRange(rangeLiq, otherSpec, 0.005) && guess.value() > IF97::psat97(temperature);};

                if (isVapor()) return calcTSpecVapor<OtherType>(temperature, otherSpec, property);
                if (isLiquid()) return calcTSpecLiquid<OtherType>(temperature, otherSpec, property, guess);
                return calcTSpecSaturation<OtherType>(temperature, otherSpec, property);
            }
            else {    // Check if we are in the saturation region
                if (isInRange(rangeVap, otherSpec)) return calcTSpecVapor<OtherType>(temperature, otherSpec, property);
                if (isInRange(rangeLiq, otherSpec, 0.005)) return calcTSpecLiquid<OtherType>(temperature, otherSpec, property);
                return calcTSpecSaturation<OtherType>(temperature, otherSpec, property);
            }
            // If we get here, something went wrong
            throw KSteamError("TV flash calculation error", "calcPropertyTV", { { "T", temperature }, { specString, otherSpec } });
        }
    }    // namespace impl

    inline FLOAT calcPropertyTRHO(FLOAT temperature, FLOAT density, Property property)
    {
        if (density > 1.0)
            return impl::calcTSpec<Property::Density>(temperature, density, property);
        else
            return impl::calcTSpec<Property::Volume>(temperature, 1.0 / density, property);
    }

    inline FLOAT calcPropertyTV(FLOAT temperature, FLOAT volume, Property property)
    {
        if (volume > 1.0)
            return impl::calcTSpec<Property::Volume>(temperature, volume, property);
        else
            return impl::calcTSpec<Property::Density>(temperature, 1.0 / volume, property);
    }

    inline FLOAT calcPropertyTH(FLOAT temperature, FLOAT enthalpy, Property property, std::optional<FLOAT> guess = std::nullopt)
    {
        return impl::calcTSpec<Property::Enthalpy>(temperature, enthalpy, property, guess);
    }

    inline FLOAT calcPropertyTS(FLOAT temperature, FLOAT entropy, Property property, std::optional<FLOAT> guess = std::nullopt)
    {
        return impl::calcTSpec<Property::Entropy>(temperature, entropy, property, guess);
    }

    inline FLOAT calcPropertyTU(FLOAT temperature, FLOAT internalEnergy, Property property, std::optional<FLOAT> guess = std::nullopt)
    {
        return impl::calcTSpec<Property::InternalEnergy>(temperature, internalEnergy, property, guess);
    }
}    // namespace XLSteam

#endif    // KSTEAM_FLASHTSPEC_HPP
