package org.basaltmc.bukkit.datacomponent;

import com.destroystokyo.paper.profile.PlayerProfile;
import com.destroystokyo.paper.profile.ProfileProperty;
import io.papermc.paper.block.BlockPredicate;
import io.papermc.paper.datacomponent.item.BannerPatternLayers;
import io.papermc.paper.datacomponent.item.BlockItemDataProperties;
import io.papermc.paper.datacomponent.item.BundleContents;
import io.papermc.paper.datacomponent.item.ChargedProjectiles;
import io.papermc.paper.datacomponent.item.Fireworks;
import io.papermc.paper.datacomponent.item.ItemAdventurePredicate;
import io.papermc.paper.datacomponent.item.ItemArmorTrim;
import io.papermc.paper.datacomponent.item.ItemContainerContents;
import io.papermc.paper.datacomponent.item.JukeboxPlayable;
import io.papermc.paper.datacomponent.item.LodestoneTracker;
import io.papermc.paper.datacomponent.item.MapDecorations;
import io.papermc.paper.datacomponent.item.MobVisibility;
import io.papermc.paper.datacomponent.item.PotDecorations;
import io.papermc.paper.datacomponent.item.PotionContents;
import io.papermc.paper.datacomponent.item.ResolvableProfile;
import io.papermc.paper.datacomponent.item.SeededContainerLoot;
import io.papermc.paper.datacomponent.item.SignText;
import io.papermc.paper.datacomponent.item.SulfurCubeContent;
import io.papermc.paper.datacomponent.item.SuspiciousStewEffects;
import io.papermc.paper.datacomponent.item.VillagerFood;
import io.papermc.paper.datacomponent.item.WritableBookContent;
import io.papermc.paper.datacomponent.item.WrittenBookContent;
import io.papermc.paper.registry.RegistryKey;
import io.papermc.paper.text.Filtered;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import net.kyori.adventure.inventory.Book;
import net.kyori.adventure.key.Key;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.ComponentLike;
import org.basaltmc.bukkit.datacomponent.Dyn.Spec;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.item.Texts;
import org.bukkit.Bukkit;
import org.bukkit.Color;
import org.bukkit.DyeColor;
import org.bukkit.FireworkEffect;
import org.bukkit.Location;
import org.bukkit.Registry;
import org.bukkit.World;
import org.bukkit.block.banner.Pattern;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.ItemType;
import org.bukkit.inventory.meta.trim.ArmorTrim;
import org.bukkit.map.MapCursor;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;
import org.bukkit.potion.PotionType;
import org.bukkit.profile.PlayerTextures;

public final class ContentSpecs {
    private ContentSpecs() {}

    private static Map<String, Object> mapOf(Object stored) {
        Map<String, Object> map = Nbt.map(stored);
        return map == null ? Nbt.compound() : map;
    }

    private static List<Object> listOf(Object stored) {
        List<Object> list = Nbt.list(stored);
        return list == null ? List.of() : list;
    }

    private static Map<String, Object> single(String name, Object value) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put(name, value);
        return fields;
    }

    private static Spec stacks(Class<?> value, Class<?> builder, String field) {
        return new Spec(value, builder).add("add", field).addAll("addAll", field).nbt(
            fields -> Wrap.stackList(Dyn.list(fields, field)), stored -> single(field, Wrap.stacksOf(stored)));
    }

    public static final Spec BUNDLE = stacks(BundleContents.class, BundleContents.Builder.class, "contents");
    public static final Spec CHARGED = stacks(ChargedProjectiles.class, ChargedProjectiles.Builder.class,
        "projectiles");

    public static final Spec CONTAINER = new Spec(ItemContainerContents.class, ItemContainerContents.Builder.class)
        .add("add", "contents").addAll("addAll", "contents").nbt(fields -> {
            List<Object> out = new ArrayList<>();
            int slot = 0;
            for (Object stack : Dyn.list(fields, "contents")) {
                if (stack != null && !((ItemStack) stack).getType().isAir()) {
                    Map<String, Object> entry = Nbt.compound();
                    entry.put("slot", slot);
                    entry.put("item", Wrap.stack((ItemStack) stack));
                    out.add(entry);
                }
                slot++;
            }
            return out;
        }, stored -> {
            List<Object> contents = new ArrayList<>();
            for (Object entry : listOf(stored)) {
                Map<String, Object> map = mapOf(entry);
                int slot = Nbt.integer(map, "slot", contents.size());
                while (contents.size() <= slot) {
                    contents.add(ItemStack.empty());
                }
                ItemStack stack = Wrap.stack(map.get("item"));
                contents.set(slot, stack == null ? ItemStack.empty() : stack);
            }
            return single("contents", contents);
        });

    public static final Spec POTION = new Spec(PotionContents.class, PotionContents.Builder.class)
        .add("addCustomEffect", "customEffects").addAll("addCustomEffects", "customEffects").call("allEffects",
            (fields, args) -> {
                List<PotionEffect> all = new ArrayList<>();
                if (fields.get("potion") instanceof PotionType potion) {
                    all.addAll(potion.getPotionEffects());
                }
                for (Object effect : Dyn.list(fields, "customEffects")) {
                    all.add((PotionEffect) effect);
                }
                return all;
            }).call("computeEffectiveColor", (fields, args) -> {
                if (fields.get("customColor") instanceof Color color) {
                    return color;
                }
                int red = 0;
                int green = 0;
                int blue = 0;
                int weight = 0;
                List<PotionEffect> all = new ArrayList<>();
                if (fields.get("potion") instanceof PotionType potion) {
                    all.addAll(potion.getPotionEffects());
                }
                for (Object effect : Dyn.list(fields, "customEffects")) {
                    all.add((PotionEffect) effect);
                }
                for (PotionEffect effect : all) {
                    if (effect.hasParticles()) {
                        Color color = effect.getType().getColor();
                        int amplifier = effect.getAmplifier() + 1;
                        red += amplifier * color.getRed();
                        green += amplifier * color.getGreen();
                        blue += amplifier * color.getBlue();
                        weight += amplifier;
                    }
                }
                return weight == 0 ? Color.fromRGB(0x385DC6) : Color.fromRGB(red / weight, green / weight,
                    blue / weight);
            }).nbt(fields -> {
                Map<String, Object> out = Nbt.compound();
                if (fields.get("potion") instanceof PotionType potion) {
                    out.put("potion", potion.getKey().toString());
                }
                if (fields.get("customColor") instanceof Color color) {
                    out.put("custom_color", color.asRGB());
                }
                List<Object> effects = new ArrayList<>();
                for (Object effect : Dyn.list(fields, "customEffects")) {
                    effects.add(Convert.effect((PotionEffect) effect));
                }
                if (!effects.isEmpty()) {
                    out.put("custom_effects", effects);
                }
                Wrap.put(out, "custom_name", fields.get("customName"));
                return out;
            }, stored -> {
                Map<String, Object> fields = new LinkedHashMap<>();
                Map<String, Object> map = stored instanceof String id ? single("potion", id) : mapOf(stored);
                if (map.get("potion") != null) {
                    fields.put("potion", Wrap.lookup(Registry.POTION, map.get("potion")));
                }
                if (map.get("custom_color") instanceof Number color) {
                    fields.put("customColor", Color.fromRGB(color.intValue() & 0xFFFFFF));
                }
                fields.put("customEffects", new ArrayList<Object>(Convert.effects(map.get("custom_effects"))));
                Wrap.put(fields, "customName", map.get("custom_name"));
                return fields;
            });

    public static final Spec DECORATIONS = new Spec(PotDecorations.class, PotDecorations.Builder.class).nbt(fields -> {
        Map<String, Object> out = Nbt.compound();
        for (String side : new String[] {"back", "left", "right", "front"}) {
            if (fields.get(side) instanceof ItemType type) {
                Map<String, Object> entry = Nbt.compound();
                entry.put("id", type.getKey().toString());
                out.put(side, entry);
            }
        }
        return out;
    }, stored -> {
        Map<String, Object> map = mapOf(stored);
        Map<String, Object> fields = new LinkedHashMap<>();
        for (String side : new String[] {"back", "left", "right", "front"}) {
            if (map.get(side) != null) {
                Object id = map.get(side) instanceof Map<?, ?> entry ? entry.get("id") : map.get(side);
                fields.put(side, Wrap.lookup(Registry.ITEM, id));
            }
        }
        return fields;
    });

    public static final Spec STEW = new Spec(SuspiciousStewEffects.class, SuspiciousStewEffects.Builder.class)
        .add("add", "effects").addAll("addAll", "effects").nbt(fields -> {
            List<Object> out = new ArrayList<>();
            for (Object effect : Dyn.list(fields, "effects")) {
                io.papermc.paper.potion.SuspiciousEffectEntry entry = (io.papermc.paper.potion.SuspiciousEffectEntry) effect;
                Map<String, Object> map = Nbt.compound();
                map.put("id", entry.effect().getKey().toString());
                map.put("duration", entry.duration());
                out.add(map);
            }
            return out;
        }, stored -> {
            List<Object> effects = new ArrayList<>();
            for (Object entry : listOf(stored)) {
                Map<String, Object> map = mapOf(entry);
                PotionEffectType type = PotionEffectType.getByKey(
                    org.bukkit.NamespacedKey.fromString(Nbt.string(map, "id", "")));
                if (type != null) {
                    effects.add(io.papermc.paper.potion.SuspiciousEffectEntry.create(type,
                        Nbt.integer(map, "duration", 160)));
                }
            }
            return single("effects", effects);
        });

    public static final Spec DECORATION = new Spec(MapDecorations.DecorationEntry.class, null).nbt(fields -> {
        Map<String, Object> out = Nbt.compound();
        out.put("type", ((MapCursor.Type) fields.get("type")).getKey().toString());
        out.put("x", fields.get("x"));
        out.put("z", fields.get("z"));
        out.put("rotation", fields.get("rotation"));
        return out;
    }, stored -> {
        Map<String, Object> map = mapOf(stored);
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("type", Wrap.lookup(Registry.MAP_DECORATION_TYPE, map.get("type")));
        fields.put("x", Nbt.real(map, "x", 0));
        fields.put("z", Nbt.real(map, "z", 0));
        fields.put("rotation", (float) Nbt.real(map, "rotation", 0));
        return fields;
    });

    public static Object decoration(MapCursor.Type type, double x, double z, float rotation) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("type", type);
        fields.put("x", x);
        fields.put("z", z);
        fields.put("rotation", rotation);
        return DECORATION.value(fields);
    }

    public static final Spec MAP_DECORATIONS = new Spec(MapDecorations.class, MapDecorations.Builder.class)
        .put("put", "decorations").putAll("putAll", "decorations").call("decoration",
            (fields, args) -> Dyn.map(fields, "decorations").get(args[0])).nbt(fields -> {
                Map<String, Object> out = Nbt.compound();
                for (Map.Entry<Object, Object> entry : Dyn.map(fields, "decorations").entrySet()) {
                    out.put((String) entry.getKey(), ((Dyn.Value) entry.getValue()).nbt());
                }
                return out;
            }, stored -> {
                Map<Object, Object> decorations = new LinkedHashMap<>();
                for (Map.Entry<String, Object> entry : mapOf(stored).entrySet()) {
                    decorations.put(entry.getKey(), DECORATION.read(entry.getValue()));
                }
                return single("decorations", decorations);
            });

    public static final Spec LOOT = new Spec(SeededContainerLoot.class, SeededContainerLoot.Builder.class)
        .def("seed", 0L).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            Wrap.putKey(out, "loot_table", (Key) fields.get("lootTable"));
            out.put("seed", fields.get("seed"));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("lootTable", Wrap.key(map.get("loot_table")));
            fields.put("seed", Nbt.longValue(map, "seed", 0));
            return fields;
        });

    private static Object filteredNbt(Filtered<?> value, java.util.function.Function<Object, Object> encode) {
        if (value.filtered() == null) {
            return encode.apply(value.raw());
        }
        Map<String, Object> out = Nbt.compound();
        out.put("raw", encode.apply(value.raw()));
        out.put("filtered", encode.apply(value.filtered()));
        return out;
    }

    private static Filtered<Object> filteredOf(Object stored, java.util.function.Function<Object, Object> decode) {
        if (stored instanceof Map<?, ?> map && map.containsKey("raw")) {
            return Filtered.of(decode.apply(map.get("raw")), map.containsKey("filtered") ? decode.apply(map.get("filtered"))
                : null);
        }
        return Filtered.of(decode.apply(stored), null);
    }

    private static Object plain(Object value) {
        return value;
    }

    private static Book book(Object title, Object author, List<Component> pages) {
        return Book.book(Component.text(String.valueOf(title)), Component.text(String.valueOf(author)), pages);
    }

    public static final Spec WRITTEN = new Spec(WrittenBookContent.class, WrittenBookContent.Builder.class)
        .def("author", "").def("generation", 0).def("resolved", false).def("title", Filtered.of("", null))
        .op("title", (fields, args) -> fields.put("title", Filtered.of(args[0], null)))
        .set("filteredTitle", "title").op("addPage", (fields, args) -> Dyn.list(fields, "pages").add(
            Filtered.of(((ComponentLike) args[0]).asComponent(), null))).op("addPages", (fields, args) -> {
                for (Object page : (List<?>) args[0]) {
                    Dyn.list(fields, "pages").add(Filtered.of(((ComponentLike) page).asComponent(), null));
                }
            }).op("addFilteredPage", (fields, args) -> Dyn.list(fields, "pages").add(componentPage(args[0])))
        .op("addFilteredPages", (fields, args) -> {
            for (Object page : (List<?>) args[0]) {
                Dyn.list(fields, "pages").add(componentPage(page));
            }
        }).call("asBook", (fields, args) -> {
            List<Component> pages = new ArrayList<>();
            for (Object page : Dyn.list(fields, "pages")) {
                pages.add((Component) ((Filtered<?>) page).raw());
            }
            return book(((Filtered<?>) fields.get("title")).raw(), fields.get("author"), pages);
        }).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("title", filteredNbt((Filtered<?>) fields.get("title"), ContentSpecs::plain));
            out.put("author", fields.get("author"));
            out.put("generation", fields.get("generation"));
            List<Object> pages = new ArrayList<>();
            for (Object page : Dyn.list(fields, "pages")) {
                pages.add(filteredNbt((Filtered<?>) page, value -> Texts.toNbt((Component) value)));
            }
            out.put("pages", pages);
            out.put("resolved", Wrap.bool((Boolean) fields.get("resolved")));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("title", filteredOf(map.get("title"), ContentSpecs::plain));
            fields.put("author", Nbt.string(map, "author", ""));
            fields.put("generation", Nbt.integer(map, "generation", 0));
            List<Object> pages = new ArrayList<>();
            for (Object page : listOf(map.get("pages"))) {
                pages.add(filteredOf(page, value -> Texts.fromNbt(value)));
            }
            fields.put("pages", pages);
            fields.put("resolved", Nbt.flag(map, "resolved", false));
            return fields;
        });

    private static Filtered<Object> componentPage(Object given) {
        Filtered<?> page = (Filtered<?>) given;
        return Filtered.of(((ComponentLike) page.raw()).asComponent(),
            page.filtered() == null ? null : ((ComponentLike) page.filtered()).asComponent());
    }

    public static final Spec WRITABLE = new Spec(WritableBookContent.class, WritableBookContent.Builder.class)
        .op("addPage", (fields, args) -> Dyn.list(fields, "pages").add(Filtered.of(args[0], null)))
        .op("addPages", (fields, args) -> {
            for (Object page : (List<?>) args[0]) {
                Dyn.list(fields, "pages").add(Filtered.of(page, null));
            }
        }).add("addFilteredPage", "pages").addAll("addFilteredPages", "pages").call("asBook", (fields, args) -> {
            List<Component> pages = new ArrayList<>();
            for (Object page : Dyn.list(fields, "pages")) {
                pages.add(Component.text(String.valueOf(((Filtered<?>) page).raw())));
            }
            return book("", "", pages);
        }).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            List<Object> pages = new ArrayList<>();
            for (Object page : Dyn.list(fields, "pages")) {
                pages.add(filteredNbt((Filtered<?>) page, ContentSpecs::plain));
            }
            out.put("pages", pages);
            return out;
        }, stored -> {
            List<Object> pages = new ArrayList<>();
            for (Object page : listOf(mapOf(stored).get("pages"))) {
                pages.add(filteredOf(page, ContentSpecs::plain));
            }
            return single("pages", pages);
        });

    public static final Spec TRIM = new Spec(ItemArmorTrim.class, ItemArmorTrim.Builder.class).nbt(fields -> {
        ArmorTrim trim = (ArmorTrim) fields.get("armorTrim");
        Map<String, Object> out = Nbt.compound();
        out.put("material", trim.getMaterial().getKey().toString());
        out.put("pattern", trim.getPattern().getKey().toString());
        return out;
    }, stored -> {
        Map<String, Object> map = mapOf(stored);
        return single("armorTrim", new ArmorTrim(Wrap.lookup(Registry.TRIM_MATERIAL, map.get("material")),
            Wrap.lookup(Registry.TRIM_PATTERN, map.get("pattern"))));
    });

    public static final Spec LODESTONE = new Spec(LodestoneTracker.class, LodestoneTracker.Builder.class)
        .def("tracked", true).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            if (fields.get("location") instanceof Location at && at.getWorld() != null) {
                Map<String, Object> target = Nbt.compound();
                target.put("pos", new int[] {at.getBlockX(), at.getBlockY(), at.getBlockZ()});
                target.put("dimension", at.getWorld().getKey().toString());
                out.put("target", target);
            }
            out.put("tracked", Wrap.bool((Boolean) fields.get("tracked")));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            Map<String, Object> target = Nbt.map(map.get("target"));
            if (target != null && target.get("pos") instanceof int[] pos && pos.length == 3) {
                World world = Bukkit.getWorld(org.bukkit.NamespacedKey.fromString(Nbt.string(target, "dimension", "")));
                fields.put("location", new Location(world, pos[0], pos[1], pos[2]));
            }
            fields.put("tracked", Nbt.flag(map, "tracked", true));
            return fields;
        });

    public static final Spec FIREWORKS = new Spec(Fireworks.class, Fireworks.Builder.class)
        .add("addEffect", "effects").addAll("addEffects", "effects").nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("flight_duration", ((Number) fields.getOrDefault("flightDuration", 0)).byteValue());
            List<Object> explosions = new ArrayList<>();
            for (Object effect : Dyn.list(fields, "effects")) {
                explosions.add(Convert.firework((FireworkEffect) effect));
            }
            out.put("explosions", explosions);
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("flightDuration", Nbt.integer(map, "flight_duration", 0));
            List<Object> effects = new ArrayList<>();
            for (Object explosion : listOf(map.get("explosions"))) {
                effects.add(Convert.firework(mapOf(explosion)));
            }
            fields.put("effects", effects);
            return fields;
        });

    public static final Spec LAYERS = new Spec(BannerPatternLayers.class, BannerPatternLayers.Builder.class)
        .add("add", "patterns").addAll("addAll", "patterns").nbt(fields -> {
            List<Object> out = new ArrayList<>();
            for (Object layer : Dyn.list(fields, "patterns")) {
                Pattern pattern = (Pattern) layer;
                Map<String, Object> entry = Nbt.compound();
                entry.put("pattern", pattern.getPattern().getKey().toString());
                entry.put("color", Convert.dye(pattern.getColor()));
                out.add(entry);
            }
            return out;
        }, stored -> {
            List<Object> patterns = new ArrayList<>();
            for (Object layer : listOf(stored)) {
                Map<String, Object> map = mapOf(layer);
                patterns.add(new Pattern(Convert.dye(Nbt.string(map, "color", "white")),
                    Wrap.lookup(Registry.BANNER_PATTERN, map.get("pattern"))));
            }
            return single("patterns", patterns);
        });

    public static final Spec BLOCK_DATA = new Spec(BlockItemDataProperties.class, BlockItemDataProperties.Builder.class)
        .put("put", "properties").putAll("putAll", "properties").call("applyTo", (fields, args) -> {
            org.bukkit.block.data.BlockData data = (org.bukkit.block.data.BlockData) args[0];
            return data.merge(properties(data.getMaterial(), fields));
        }).call("createBlockData", (fields, args) -> {
            org.bukkit.Material material = ((org.bukkit.block.BlockType) args[0]).asMaterial();
            return properties(material, fields);
        }).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            for (Map.Entry<Object, Object> entry : Dyn.map(fields, "properties").entrySet()) {
                out.put((String) entry.getKey(), String.valueOf(entry.getValue()));
            }
            return out;
        }, stored -> {
            Map<Object, Object> properties = new LinkedHashMap<>();
            for (Map.Entry<String, Object> entry : mapOf(stored).entrySet()) {
                properties.put(entry.getKey(), String.valueOf(entry.getValue()));
            }
            return single("properties", properties);
        });

    private static org.bukkit.block.data.BlockData properties(org.bukkit.Material material, Map<String, Object> fields) {
        StringBuilder text = new StringBuilder("[");
        for (Map.Entry<Object, Object> entry : Dyn.map(fields, "properties").entrySet()) {
            text.append(text.length() > 1 ? "," : "").append(entry.getKey()).append('=').append(entry.getValue());
        }
        return Bukkit.createBlockData(material, text.append(']').toString());
    }

    public static final Spec JUKEBOX = new Spec(JukeboxPlayable.class, JukeboxPlayable.Builder.class).nbt(
        fields -> ((org.bukkit.JukeboxSong) fields.get("jukeboxSong")).getKey().toString(),
        stored -> single("jukeboxSong", Wrap.lookup(Registry.JUKEBOX_SONG,
            stored instanceof Map<?, ?> map ? map.get("song") : stored)));

    public static final Spec PREDICATE = new Spec(ItemAdventurePredicate.class, ItemAdventurePredicate.Builder.class)
        .add("addPredicate", "predicates").addAll("addPredicates", "predicates").nbt(fields -> {
            List<Object> predicates = new ArrayList<>();
            for (Object predicate : Dyn.list(fields, "predicates")) {
                Map<String, Object> entry = Nbt.compound();
                entry.put("blocks", Wrap.setNbt(((BlockPredicate) predicate).blocks()));
                predicates.add(entry);
            }
            Map<String, Object> out = Nbt.compound();
            out.put("predicates", predicates);
            return out;
        }, stored -> {
            List<Object> predicates = new ArrayList<>();
            Object raw = stored instanceof Map<?, ?> map ? map.get("predicates") : stored;
            for (Object predicate : listOf(raw)) {
                Object blocks = predicate instanceof Map<?, ?> entry ? entry.get("blocks") : predicate;
                predicates.add(BlockPredicate.predicate().blocks(Wrap.set(RegistryKey.BLOCK, blocks)).build());
            }
            return single("predicates", predicates);
        });

    public static final Spec SULFUR = new Spec(SulfurCubeContent.class, null).nbt(
        fields -> Wrap.stack((ItemStack) fields.get("absorbedItem")),
        stored -> single("absorbedItem", Wrap.stack(stored)));

    public static final Spec VISIBILITY = new Spec(MobVisibility.class, null).nbt(fields -> {
        Map<String, Object> out = Nbt.compound();
        out.put("targeting_entity_types",
            Wrap.setNbt((io.papermc.paper.registry.set.RegistryKeySet<?>) fields.get("targetingEntityTypes")));
        out.put("visibility", fields.get("visibility"));
        return out;
    }, stored -> {
        Map<String, Object> map = mapOf(stored);
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("targetingEntityTypes", Wrap.set(RegistryKey.ENTITY_TYPE, map.get("targeting_entity_types")));
        fields.put("visibility", (float) Nbt.real(map, "visibility", 1));
        return fields;
    });

    public static final Spec FOOD = new Spec(VillagerFood.class, null).nbt(fields -> {
        Map<String, Object> out = Nbt.compound();
        out.put("nutrition", fields.get("nutrition"));
        return out;
    }, stored -> single("nutrition", Nbt.integer(mapOf(stored), "nutrition", 1)));

    public static final Spec SIGN = new Spec(SignText.class, SignText.Builder.class).def("color", DyeColor.BLACK)
        .def("hasGlowingText", false).op("lines", (fields, args) -> {
            List<Object> lines = Dyn.list(fields, "lines");
            lines.clear();
            for (Object line : (List<?>) args[0]) {
                lines.add(((ComponentLike) line).asComponent());
            }
        }).op("line", (fields, args) -> {
            List<Object> lines = Dyn.list(fields, "lines");
            int at = (Integer) args[0];
            while (lines.size() <= at) {
                lines.add(Component.empty());
            }
            lines.set(at, ((ComponentLike) args[1]).asComponent());
        }).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            List<Object> messages = new ArrayList<>();
            List<Object> lines = Dyn.list(fields, "lines");
            for (int at = 0; at < 4; at++) {
                messages.add(Texts.toNbt(at < lines.size() ? (Component) lines.get(at) : Component.empty()));
            }
            out.put("messages", messages);
            out.put("color", Convert.dye((DyeColor) fields.get("color")));
            out.put("has_glowing_text", Wrap.bool((Boolean) fields.get("hasGlowingText")));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            List<Object> lines = new ArrayList<>();
            for (Object message : listOf(map.get("messages"))) {
                lines.add(Texts.fromNbt(message));
            }
            fields.put("lines", lines);
            DyeColor color = Convert.dye(Nbt.string(map, "color", "black"));
            fields.put("color", color == null ? DyeColor.BLACK : color);
            fields.put("hasGlowingText", Nbt.flag(map, "has_glowing_text", false));
            return fields;
        });

    public static final Spec PATCH = new Spec(ResolvableProfile.SkinPatch.class, ResolvableProfile.SkinPatchBuilder.class)
        .call("isEmpty", (fields, args) -> fields.get("body") == null && fields.get("cape") == null
            && fields.get("elytra") == null && fields.get("model") == null).nbt(fields -> {
                Map<String, Object> out = Nbt.compound();
                Wrap.putKey(out, "texture", (Key) fields.get("body"));
                Wrap.putKey(out, "cape", (Key) fields.get("cape"));
                Wrap.putKey(out, "elytra", (Key) fields.get("elytra"));
                if (fields.get("model") != null) {
                    out.put("model", fields.get("model").toString().toLowerCase(java.util.Locale.ROOT));
                }
                return out;
            }, stored -> {
                Map<String, Object> map = mapOf(stored);
                Map<String, Object> fields = new LinkedHashMap<>();
                if (map.get("texture") != null) {
                    fields.put("body", Wrap.key(map.get("texture")));
                }
                if (map.get("cape") != null) {
                    fields.put("cape", Wrap.key(map.get("cape")));
                }
                if (map.get("elytra") != null) {
                    fields.put("elytra", Wrap.key(map.get("elytra")));
                }
                if (map.get("model") != null) {
                    fields.put("model", PlayerTextures.SkinModel.valueOf(
                        String.valueOf(map.get("model")).toUpperCase(java.util.Locale.ROOT)));
                }
                return fields;
            });

    public static final Spec PROFILE = new Spec(ResolvableProfile.class, ResolvableProfile.Builder.class)
        .def("skinPatch", PATCH.value(new LinkedHashMap<>())).add("addProperty", "properties")
        .addAll("addProperties", "properties").op("skinPatch", (fields, args) -> {
            if (args[0] instanceof java.util.function.Consumer<?> consumer) {
                Object builder = PATCH.builder();
                @SuppressWarnings("unchecked")
                java.util.function.Consumer<Object> typed = (java.util.function.Consumer<Object>) consumer;
                typed.accept(builder);
                fields.put("skinPatch", ((io.papermc.paper.datacomponent.DataComponentBuilder<?>) builder).build());
            } else {
                fields.put("skinPatch", args[0]);
            }
        }).call("dynamic", (fields, args) -> fields.get("name") == null && fields.get("uuid") != null
            || fields.get("name") != null && fields.get("uuid") == null).call("resolve", (fields, args) -> {
                PlayerProfile profile = Bukkit.createProfile((UUID) fields.get("uuid"), (String) fields.get("name"));
                for (Object property : Dyn.list(fields, "properties")) {
                    profile.setProperty((ProfileProperty) property);
                }
                return CompletableFuture.completedFuture(profile);
            }).call("applySkinToPlayerHeadContents", (fields, args) -> {
                net.kyori.adventure.text.object.PlayerHeadObjectContents.Builder builder =
                    (net.kyori.adventure.text.object.PlayerHeadObjectContents.Builder) args[0];
                if (fields.get("name") != null) {
                    builder.name((String) fields.get("name"));
                }
                if (fields.get("uuid") != null) {
                    builder.id((UUID) fields.get("uuid"));
                }
                for (Object property : Dyn.list(fields, "properties")) {
                    ProfileProperty held = (ProfileProperty) property;
                    builder.profileProperty(net.kyori.adventure.text.object.PlayerHeadObjectContents
                        .property(held.getName(), held.getValue(), held.getSignature()));
                }
                return null;
            }).nbt(fields -> {
                Map<String, Object> out = Nbt.compound();
                Wrap.put(out, "name", fields.get("name"));
                if (fields.get("uuid") instanceof UUID id) {
                    out.put("id", Convert.uuidToInts(id));
                }
                List<Object> properties = new ArrayList<>();
                for (Object property : Dyn.list(fields, "properties")) {
                    ProfileProperty held = (ProfileProperty) property;
                    Map<String, Object> entry = Nbt.compound();
                    entry.put("name", held.getName());
                    entry.put("value", held.getValue());
                    Wrap.put(entry, "signature", held.getSignature());
                    properties.add(entry);
                }
                if (!properties.isEmpty()) {
                    out.put("properties", properties);
                }
                Object patch = fields.get("skinPatch");
                if (patch != null && !((ResolvableProfile.SkinPatch) patch).isEmpty()) {
                    out.putAll(mapOf(((Dyn.Value) patch).nbt()));
                }
                return out;
            }, stored -> {
                Map<String, Object> map = stored instanceof String name ? single("name", name) : mapOf(stored);
                Map<String, Object> fields = new LinkedHashMap<>();
                Wrap.put(fields, "name", map.get("name"));
                if (map.get("id") != null) {
                    fields.put("uuid", Convert.uuidOf(map.get("id")));
                }
                List<Object> properties = new ArrayList<>();
                for (Object entry : listOf(map.get("properties"))) {
                    Map<String, Object> property = mapOf(entry);
                    properties.add(new ProfileProperty(Nbt.string(property, "name", ""),
                        Nbt.string(property, "value", ""), Nbt.string(property, "signature", null)));
                }
                fields.put("properties", properties);
                fields.put("skinPatch", PATCH.read(map));
                return fields;
            });
}
