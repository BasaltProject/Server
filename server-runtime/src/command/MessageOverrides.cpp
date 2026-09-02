#include <basalt/server/runtime/command/MessageOverrides.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/wire/common/placeholder/PlaceholderText.hpp>
#include <basalt/wire/text/MiniMessage.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <utility>

namespace basalt::server::runtime::command {
namespace {

using wire::text::Component;

} // namespace

MessageOverrides::MessageOverrides(std::map<std::string, std::string, std::less<>> messages,
                                   const wire::common::placeholder::PlaceholderRegistry& placeholders,
                                   wire::text::LangTable lang)
    : messages_(std::move(messages)), placeholders_(&placeholders), lang_(std::move(lang)) {}

Component MessageOverrides::apply(const Component& message, const player::PlayerSession* player) const {
    return messages_.empty() ? message : transform(message, player);
}

Component MessageOverrides::transform(const Component& component, const player::PlayerSession* player) const {
    const auto* translatable = std::get_if<Component::Translatable>(&component.payload());
    if (translatable != nullptr) {
        const auto override = messages_.find(translatable->key);
        std::vector<Component> args;
        args.reserve(translatable->args.size());
        for (const Component& argument : translatable->args) {
            args.push_back(transform(argument, player));
        }
        if (override != messages_.end()) {
            const Component rendered = render(override->second, args, player);
            if (component.children().empty()) {
                return rendered;
            }
            std::vector<Component> children;
            children.reserve(component.children().size());
            for (const Component& child : component.children()) {
                children.push_back(transform(child, player));
            }
            return rendered.withChildren(std::move(children));
        }
        // Not overridden itself, but its arguments can be: `chat.type.admin` carries the whole
        // command message as an argument, so a transform that only walked children would leave
        // exactly the copy every other operator sees untouched.
        std::vector<Component> children;
        children.reserve(component.children().size());
        for (const Component& child : component.children()) {
            children.push_back(transform(child, player));
        }
        return Component{Component::Translatable{
                             .key = translatable->key, .args = std::move(args), .fallback = translatable->fallback},
                         component.style(), std::move(children)};
    }
    if (component.children().empty()) {
        return component;
    }
    std::vector<Component> children;
    children.reserve(component.children().size());
    for (const Component& child : component.children()) {
        children.push_back(transform(child, player));
    }
    return component.withChildren(std::move(children));
}

Component MessageOverrides::render(const std::string& text, const std::vector<Component>& args,
                                   const player::PlayerSession* player) const {
    // The operator's text: arguments first, then placeholders, then MiniMessage.
    //
    // Arguments come in as components and go in as plain text. Keeping their own colours would mean
    // splicing components into a MiniMessage parse, and the result would fight whatever styling the
    // operator wrote around them; a name is a name.
    std::string result = text;
    for (std::size_t index = 0; index < args.size(); index++) {
        const std::string token = "%" + std::to_string(index + 1) + "%";
        std::size_t at = result.find(token);
        if (at == std::string::npos) {
            continue;
        }
        const std::string plain = wire::text::resolvePlainText(args[index], lang_);
        while (at != std::string::npos) {
            result.replace(at, token.size(), plain);
            at = result.find(token, at + plain.size());
        }
    }
    const wire::common::placeholder::PlaceholderContext context =
        wire::common::placeholder::PlaceholderContext{}.with(placeholder::playerSlot(), player);
    return wire::text::miniMessage::parse(wire::common::placeholder::substitute(result, *placeholders_, context));
}

} // namespace basalt::server::runtime::command
