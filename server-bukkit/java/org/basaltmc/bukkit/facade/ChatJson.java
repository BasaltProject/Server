package org.basaltmc.bukkit.facade;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonNull;
import com.google.gson.JsonObject;
import com.google.gson.JsonPrimitive;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.JsonOps;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.MapLike;
import com.mojang.serialization.RecordBuilder;
import java.net.URI;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.function.Function;
import java.util.stream.Stream;
import net.minecraft.ChatFormatting;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.ClickEvent;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.FontDescription;
import net.minecraft.network.chat.HoverEvent;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.chat.Style;
import net.minecraft.network.chat.TextColor;
import net.minecraft.network.chat.contents.KeybindContents;
import net.minecraft.network.chat.contents.PlainTextContents;
import net.minecraft.network.chat.contents.TranslatableContents;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;

/** Converts between the facade component model and the modern JSON text format. */
public final class ChatJson {
    private static final List<String> STYLE_KEYS = List.of("color", "shadow_color", "bold", "italic", "underlined",
        "strikethrough", "obfuscated", "click_event", "hover_event", "insertion", "font");
    private static final List<String> CONTENT_KEYS = List.of("text", "translate", "fallback", "with", "keybind", "score", "selector", "separator");

    public static final Codec<Tag> TAG_CODEC = Codec.PASSTHROUGH.xmap(
        dynamic -> dynamic.convert(NbtOps.INSTANCE).getValue(), tag -> new Dynamic<>(NbtOps.INSTANCE, tag));

    private ChatJson() {}

    public static <T> MapCodec<T> unsupportedMap(String what) {
        return new MapCodec<>() {
            @Override
            public <O> DataResult<T> decode(DynamicOps<O> ops, MapLike<O> input) {
                return DataResult.error(() -> what + " are not supported by Basalt");
            }

            @Override
            public <O> RecordBuilder<O> encode(T input, DynamicOps<O> ops, RecordBuilder<O> prefix) {
                return prefix.withErrorsFrom(DataResult.error(() -> what + " are not supported by Basalt"));
            }

            @Override
            public <O> Stream<O> keys(DynamicOps<O> ops) {
                return Stream.empty();
            }
        };
    }

    private static <T> MapCodec<T> jsonMap(List<String> keys, Function<JsonObject, DataResult<T>> decode,
        Function<T, JsonObject> encode) {
        return new MapCodec<>() {
            @Override
            public <O> DataResult<T> decode(DynamicOps<O> ops, MapLike<O> input) {
                JsonObject object = new JsonObject();
                for (String key : keys) {
                    O value = input.get(key);
                    if (value != null) {
                        object.add(key, new Dynamic<>(ops, value).convert(JsonOps.INSTANCE).getValue());
                    }
                }
                return decode.apply(object);
            }

            @Override
            public <O> RecordBuilder<O> encode(T value, DynamicOps<O> ops, RecordBuilder<O> prefix) {
                JsonObject object = encode.apply(value);
                for (var entry : object.entrySet()) {
                    prefix.add(entry.getKey(), new Dynamic<>(JsonOps.INSTANCE, entry.getValue()).convert(ops).getValue());
                }
                return prefix;
            }

            @Override
            public <O> Stream<O> keys(DynamicOps<O> ops) {
                return keys.stream().map(ops::createString);
            }
        };
    }

    public static MapCodec<Style> styleMapCodec() {
        return jsonMap(STYLE_KEYS, object -> {
            try {
                return DataResult.success(styleFromJson(object));
            } catch (RuntimeException e) {
                return DataResult.error(e::getMessage);
            }
        }, ChatJson::styleToJson);
    }

    public static MapCodec<TranslatableContents> translatableMapCodec() {
        return jsonMap(CONTENT_KEYS, object -> {
            try {
                Component component = fromJson(object);
                if (component.getContents() instanceof TranslatableContents translatable) {
                    return DataResult.success(translatable);
                }
                return DataResult.error(() -> "Not a translatable component");
            } catch (RuntimeException e) {
                return DataResult.error(e::getMessage);
            }
        }, contents -> {
            JsonObject object = new JsonObject();
            writeContents(object, contents);
            return object;
        });
    }

    public static Codec<Component> componentCodec() {
        return Codec.PASSTHROUGH.flatXmap(dynamic -> {
            try {
                return DataResult.success(fromJson(dynamic.convert(JsonOps.INSTANCE).getValue()));
            } catch (RuntimeException e) {
                return DataResult.error(() -> "Failed to parse component: " + e.getMessage());
            }
        }, component -> DataResult.success(new Dynamic<>(JsonOps.INSTANCE, toJson(component))));
    }

    public static <T> StreamCodec<RegistryFriendlyByteBuf, T> nbtStream(Codec<T> codec) {
        return StreamCodec.of(
            (buf, value) -> buf.writeNbt(codec.encodeStart(NbtOps.INSTANCE, value).getOrThrow()),
            buf -> codec.parse(NbtOps.INSTANCE, buf.readNbt(net.minecraft.nbt.NbtAccounter.defaultQuota())).getOrThrow());
    }

    public static net.kyori.adventure.text.Component toAdventure(Component component) {
        return net.kyori.adventure.text.serializer.gson.GsonComponentSerializer.gson()
            .deserializeFromTree(toJson(component));
    }

    public static Component fromAdventure(net.kyori.adventure.text.Component component) {
        return fromJson(net.kyori.adventure.text.serializer.gson.GsonComponentSerializer.gson()
            .serializeToTree(component));
    }

    public static JsonElement toJson(Component component) {
        JsonObject object = new JsonObject();
        writeContents(object, component.getContents());
        JsonObject style = styleToJson(component.getStyle());
        for (var entry : style.entrySet()) {
            object.add(entry.getKey(), entry.getValue());
        }
        if (!component.getSiblings().isEmpty()) {
            JsonArray extra = new JsonArray();
            for (Component sibling : component.getSiblings()) {
                extra.add(toJson(sibling));
            }
            object.add("extra", extra);
        }
        return object;
    }

    private static void writeContents(JsonObject object, net.minecraft.network.chat.ComponentContents contents) {
        if (contents instanceof PlainTextContents plain) {
            object.addProperty("text", plain.text());
        } else if (contents instanceof TranslatableContents translatable) {
            object.addProperty("translate", translatable.getKey());
            if (translatable.getFallback() != null) {
                object.addProperty("fallback", translatable.getFallback());
            }
            if (translatable.getArgs().length > 0) {
                JsonArray with = new JsonArray();
                for (Object arg : translatable.getArgs()) {
                    with.add(arg instanceof Component component ? toJson(component) : primitive(arg));
                }
                object.add("with", with);
            }
        } else if (contents instanceof KeybindContents keybind) {
            object.addProperty("keybind", keybind.getName());
        } else if (contents instanceof net.minecraft.network.chat.contents.ScoreContents score) {
            JsonObject inner = new JsonObject();
            inner.addProperty("name", score.name().<String>map(selector -> selector.source(), plain -> plain));
            inner.addProperty("objective", score.objective());
            object.add("score", inner);
        } else if (contents instanceof net.minecraft.network.chat.contents.SelectorContents selector) {
            object.addProperty("selector", selector.selector().source());
            selector.separator().ifPresent(separator -> object.add("separator", toJson(separator)));
        } else if (contents instanceof net.minecraft.network.chat.contents.NbtContents nbt) {
            copyInto(object, net.minecraft.network.chat.contents.NbtContents.MAP_CODEC.codec().encodeStart(JsonOps.INSTANCE, nbt).getOrThrow());
        } else if (contents instanceof net.minecraft.network.chat.contents.ObjectContents sprite) {
            copyInto(object, net.minecraft.network.chat.contents.ObjectContents.MAP_CODEC.codec().encodeStart(JsonOps.INSTANCE, sprite).getOrThrow());
        } else {
            throw new IllegalArgumentException("Unsupported component contents " + contents);
        }
    }

    private static void copyInto(JsonObject target, JsonElement encoded) {
        for (var entry : encoded.getAsJsonObject().entrySet()) {
            target.add(entry.getKey(), entry.getValue());
        }
    }

    private static JsonElement primitive(Object value) {
        if (value instanceof Number number) {
            return new JsonPrimitive(number);
        }
        if (value instanceof Boolean flag) {
            return new JsonPrimitive(flag);
        }
        return new JsonPrimitive(String.valueOf(value));
    }

    public static Component fromJson(JsonElement element) {
        if (element.isJsonPrimitive()) {
            return Component.literal(element.getAsString());
        }
        if (element.isJsonArray()) {
            JsonArray array = element.getAsJsonArray();
            if (array.isEmpty()) {
                throw new IllegalArgumentException("Empty component array");
            }
            MutableComponent first = fromJson(array.get(0)).copy();
            for (int i = 1; i < array.size(); i++) {
                first.append(fromJson(array.get(i)));
            }
            return first;
        }
        JsonObject object = element.getAsJsonObject();
        MutableComponent result;
        if (object.has("text")) {
            result = Component.literal(object.get("text").getAsString());
        } else if (object.has("translate")) {
            List<Object> args = new ArrayList<>();
            if (object.has("with")) {
                for (JsonElement arg : object.getAsJsonArray("with")) {
                    if (arg.isJsonPrimitive()) {
                        JsonPrimitive primitive = arg.getAsJsonPrimitive();
                        args.add(primitive.isBoolean() ? (Object) primitive.getAsBoolean()
                            : primitive.isNumber() ? (Object) primitive.getAsNumber() : primitive.getAsString());
                    } else {
                        args.add(fromJson(arg));
                    }
                }
            }
            String fallback = object.has("fallback") ? object.get("fallback").getAsString() : null;
            result = MutableComponent.create(
                new TranslatableContents(object.get("translate").getAsString(), fallback, args.toArray()));
        } else if (object.has("keybind")) {
            result = Component.keybind(object.get("keybind").getAsString());
        } else if (object.has("score")) {
            JsonObject inner = object.getAsJsonObject("score");
            String holder = inner.get("name").getAsString();
            String objective = inner.get("objective").getAsString();
            result = holder.startsWith("@")
                ? Component.score(selectorOf(holder), objective) : Component.score(holder, objective);
        } else if (object.has("selector")) {
            result = Component.selector(selectorOf(object.get("selector").getAsString()),
                object.has("separator") ? Optional.of(fromJson(object.get("separator"))) : Optional.empty());
        } else if (object.has("nbt")) {
            result = MutableComponent.create(net.minecraft.network.chat.contents.NbtContents.MAP_CODEC.codec().parse(JsonOps.INSTANCE, object).getOrThrow());
        } else if (object.has("object") || object.has("sprite") || object.has("player")) {
            result = MutableComponent.create(net.minecraft.network.chat.contents.ObjectContents.MAP_CODEC.codec().parse(JsonOps.INSTANCE, object).getOrThrow());
        } else {
            throw new IllegalArgumentException("Don't know how to turn " + element + " into a Component");
        }
        if (object.has("extra")) {
            for (JsonElement extra : object.getAsJsonArray("extra")) {
                result.append(fromJson(extra));
            }
        }
        return result.setStyle(styleFromJson(object));
    }

    public static JsonObject styleToJson(Style style) {
        JsonObject object = new JsonObject();
        if (style.getColor() != null) {
            object.addProperty("color", style.getColor().serialize());
        }
        if (style.getShadowColor() != null) {
            object.addProperty("shadow_color", style.getShadowColor());
        }
        flag(object, "bold", style.isBold(), style);
        flag(object, "italic", style.isItalic(), style);
        flag(object, "underlined", style.isUnderlined(), style);
        flag(object, "strikethrough", style.isStrikethrough(), style);
        flag(object, "obfuscated", style.isObfuscated(), style);
        if (style.getClickEvent() != null) {
            object.add("click_event", ClickEvent.CODEC.encodeStart(JsonOps.INSTANCE, style.getClickEvent()).getOrThrow());
        }
        if (style.getHoverEvent() != null) {
            object.add("hover_event", HoverEvent.CODEC.encodeStart(JsonOps.INSTANCE, style.getHoverEvent()).getOrThrow());
        }
        if (style.getInsertion() != null) {
            object.addProperty("insertion", style.getInsertion());
        }
        if (style.getFont() instanceof FontDescription.Resource resource) {
            object.addProperty("font", resource.id().toString());
        }
        return object;
    }

    private static void flag(JsonObject object, String name, boolean value, Style style) {
        if (value) {
            object.addProperty(name, true);
        }
    }

    public static Style styleFromJson(JsonObject object) {
        Style style = Style.EMPTY;
        if (object.has("color")) {
            style = style.withColor(TextColor.parseColor(object.get("color").getAsString())
                .getOrThrow(IllegalArgumentException::new));
        }
        if (object.has("shadow_color")) {
            JsonElement shadow = object.get("shadow_color");
            style = style.withShadowColor(shadow.isJsonArray() ? shadowFromFloats(shadow.getAsJsonArray()) : shadow.getAsInt());
        }
        if (object.has("bold")) {
            style = style.withBold(asBoolean(object.get("bold")));
        }
        if (object.has("italic")) {
            style = style.withItalic(asBoolean(object.get("italic")));
        }
        if (object.has("underlined")) {
            style = style.withUnderlined(asBoolean(object.get("underlined")));
        }
        if (object.has("strikethrough")) {
            style = style.withStrikethrough(asBoolean(object.get("strikethrough")));
        }
        if (object.has("obfuscated")) {
            style = style.withObfuscated(asBoolean(object.get("obfuscated")));
        }
        if (object.has("click_event")) {
            style = style.withClickEvent(ClickEvent.CODEC.parse(JsonOps.INSTANCE, object.get("click_event")).getOrThrow());
        }
        if (object.has("hover_event")) {
            style = style.withHoverEvent(HoverEvent.CODEC.parse(JsonOps.INSTANCE, object.get("hover_event")).getOrThrow());
        }
        if (object.has("insertion")) {
            style = style.withInsertion(object.get("insertion").getAsString());
        }
        if (object.has("font")) {
            style = style.withFont(new FontDescription.Resource(Identifier.parse(object.get("font").getAsString())));
        }
        return style;
    }

    private static boolean asBoolean(JsonElement element) {
        JsonPrimitive primitive = element.getAsJsonPrimitive();
        return primitive.isBoolean() ? primitive.getAsBoolean() : primitive.getAsDouble() != 0.0;
    }

    private static int shadowFromFloats(JsonArray array) {
        int a = Math.round(array.get(3).getAsFloat() * 255.0F);
        int r = Math.round(array.get(0).getAsFloat() * 255.0F);
        int g = Math.round(array.get(1).getAsFloat() * 255.0F);
        int b = Math.round(array.get(2).getAsFloat() * 255.0F);
        return a << 24 | r << 16 | g << 8 | b;
    }

    private static net.minecraft.util.CompilableString<net.minecraft.commands.arguments.selector.EntitySelector> selectorOf(
        String text) {
        return net.minecraft.util.CompilableString.codec(value -> DataResult.success(
            net.minecraft.commands.arguments.selector.EntitySelector.ofText(value,
                net.minecraft.commands.arguments.selector.EntitySelector.INFINITE, true)))
            .parse(JsonOps.INSTANCE, new JsonPrimitive(text)).getOrThrow();
    }

    public static Optional<URI> uri(String text) {
        try {
            return Optional.of(new URI(text));
        } catch (java.net.URISyntaxException e) {
            return Optional.empty();
        }
    }
}
