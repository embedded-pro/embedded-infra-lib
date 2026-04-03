#include "infra/syntax/JsonObjectNavigator.hpp"

namespace infra
{
    JsonObjectNavigator::JsonObjectNavigator(const std::string& contents)
        : object(contents)
    {}

    JsonObjectNavigator::JsonObjectNavigator(const infra::JsonObject& object)
        : object(object)
    {}

    JsonObjectNavigator JsonObjectNavigator::operator/(const JsonObjectNavigatorToken& token) const
    {
        auto subObject = object.GetOptionalObject(token.name);
        if (subObject == std::nullopt)
            throw std::runtime_error(("Object " + token.name + " not found").c_str());

        return { *subObject };
    }

    JsonOptionalObjectNavigator JsonObjectNavigator::operator/(const JsonOptionalObjectNavigatorToken& token) const
    {
        auto subObject = object.GetOptionalObject(token.name);
        if (subObject == std::nullopt)
            return {};

        return { *subObject };
    }

    JsonArrayNavigator JsonObjectNavigator::operator/(const JsonArrayNavigatorToken& token) const
    {
        auto subArray = object.GetOptionalArray(token.name);
        if (subArray == std::nullopt)
            throw std::runtime_error(("Array " + token.name + " not found").c_str());

        return { *subArray };
    }

    JsonOptionalArrayNavigator JsonObjectNavigator::operator/(const JsonOptionalArrayNavigatorToken& token) const
    {
        auto subArray = object.GetOptionalArray(token.name);
        if (subArray == std::nullopt)
            return {};

        return { *subArray };
    }

    std::string JsonObjectNavigator::operator/(const JsonStringNavigatorToken& token) const
    {
        auto member = object.GetOptionalString(token.name);
        if (member == std::nullopt)
            throw std::runtime_error(("String " + token.name + " not found").c_str());

        return member->ToStdString();
    }

    std::optional<std::string> JsonObjectNavigator::operator/(const JsonOptionalStringNavigatorToken& token) const
    {
        auto member = object.GetOptionalString(token.name);

        return member ? std::make_optional(member->ToStdString()) : std::nullopt;
    }

    int32_t JsonObjectNavigator::operator/(const JsonIntegerNavigatorToken& token) const
    {
        auto member = object.GetOptionalInteger(token.name);
        if (member == std::nullopt)
            throw std::runtime_error(("Integer " + token.name + " not found").c_str());

        return *member;
    }

    bool JsonObjectNavigator::operator/(const JsonBoolNavigatorToken& token) const
    {
        auto member = object.GetOptionalBoolean(token.name);
        if (member == std::nullopt)
            throw std::runtime_error(("Boolean " + token.name + " not found").c_str());

        return *member;
    }

    JsonOptionalObjectNavigator::JsonOptionalObjectNavigator(infra::JsonObject& object)
        : navigator(std::in_place, object)
    {}

    JsonOptionalObjectNavigator::JsonOptionalObjectNavigator(const JsonObjectNavigator& navigator)
        : navigator(std::in_place, navigator)
    {}

    JsonOptionalObjectNavigator JsonOptionalObjectNavigator::operator/(const JsonObjectNavigatorToken& token) const
    {
        if (navigator != std::nullopt)
            return *navigator / token;
        else
            return {};
    }

    JsonOptionalObjectNavigator JsonOptionalObjectNavigator::operator/(const JsonOptionalObjectNavigatorToken& token) const
    {
        if (navigator != std::nullopt)
            return *navigator / token;
        else
            return {};
    }

    JsonOptionalArrayNavigator JsonOptionalObjectNavigator::operator/(const JsonArrayNavigatorToken& token) const
    {
        if (navigator != std::nullopt)
            return *navigator / token;
        else
            return {};
    }

    JsonOptionalArrayNavigator JsonOptionalObjectNavigator::operator/(const JsonOptionalArrayNavigatorToken& token) const
    {
        if (navigator != std::nullopt)
            return *navigator / token;
        else
            return {};
    }

    std::optional<std::string> JsonOptionalObjectNavigator::operator/(const JsonStringNavigatorToken& token) const
    {
        if (navigator != std::nullopt)
            return std::make_optional(*navigator / token);
        else
            return {};
    }

    std::optional<std::string> JsonOptionalObjectNavigator::operator/(const JsonOptionalStringNavigatorToken& token) const
    {
        if (navigator != std::nullopt)
            return *navigator / token;
        else
            return {};
    }

    std::optional<int32_t> JsonOptionalObjectNavigator::operator/(const JsonIntegerNavigatorToken& token) const
    {
        if (navigator != std::nullopt)
            return std::make_optional(*navigator / token);
        else
            return {};
    }

    std::optional<bool> JsonOptionalObjectNavigator::operator/(const JsonBoolNavigatorToken& token) const
    {
        if (navigator != std::nullopt)
            return std::make_optional(*navigator / token);
        else
            return {};
    }

    JsonArrayNavigator::JsonArrayNavigator(infra::JsonArray& array)
        : array(array)
    {}

    JsonOptionalArrayNavigator::JsonOptionalArrayNavigator(infra::JsonArray& array)
        : navigator(std::in_place, array)
    {}

    JsonOptionalArrayNavigator::JsonOptionalArrayNavigator(const JsonArrayNavigator& navigator)
        : navigator(std::in_place, navigator)
    {}
}
