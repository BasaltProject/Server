package org.basaltmc.bukkit.item;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import org.bukkit.Color;
import org.bukkit.DyeColor;
import org.bukkit.FireworkEffect;
import org.bukkit.NamespacedKey;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;

public final class Convert {
    private Convert() {}

    public static int[] uuidToInts(UUID id) {
        long most = id.getMostSignificantBits();
        long least = id.getLeastSignificantBits();
        return new int[] {(int) (most >> 32), (int) most, (int) (least >> 32), (int) least};
    }

    public static UUID uuidOf(Object value) {
        if (value instanceof int[] ints && ints.length == 4) {
            return new UUID(((long) ints[0] << 32) | (ints[1] & 0xFFFFFFFFL), ((long) ints[2] << 32) | (ints[3] & 0xFFFFFFFFL));
        }
        if (value instanceof String text) {
            try {
                return UUID.fromString(text);
            } catch (IllegalArgumentException e) {
                return null;
            }
        }
        return null;
    }

    public static List<Object> colors(List<Color> colors) {
        List<Object> out = new ArrayList<>();
        for (Color color : colors) {
            out.add(color.asRGB());
        }
        return out;
    }

    public static List<Color> colorsOf(Object value) {
        List<Color> out = new ArrayList<>();
        List<Object> list = Nbt.list(value);
        if (list != null) {
            for (Object entry : list) {
                if (entry instanceof Number number) {
                    out.add(Color.fromRGB(number.intValue() & 0xFFFFFF));
                }
            }
        }
        return out;
    }

    public static Map<String, Object> firework(FireworkEffect effect) {
        Map<String, Object> out = Nbt.compound();
        String shape;
        switch (effect.getType()) {
            case BALL_LARGE:
                shape = "large_ball";
                break;
            case STAR:
                shape = "star";
                break;
            case CREEPER:
                shape = "creeper";
                break;
            case BURST:
                shape = "burst";
                break;
            default:
                shape = "small_ball";
                break;
        }
        out.put("shape", shape);
        out.put("colors", colors(effect.getColors()));
        out.put("fade_colors", colors(effect.getFadeColors()));
        out.put("has_trail", Nbt.bool(effect.hasTrail()));
        out.put("has_twinkle", Nbt.bool(effect.hasFlicker()));
        return out;
    }

    public static FireworkEffect firework(Map<String, Object> data) {
        FireworkEffect.Type type;
        switch (Nbt.string(data, "shape", "small_ball")) {
            case "large_ball":
                type = FireworkEffect.Type.BALL_LARGE;
                break;
            case "star":
                type = FireworkEffect.Type.STAR;
                break;
            case "creeper":
                type = FireworkEffect.Type.CREEPER;
                break;
            case "burst":
                type = FireworkEffect.Type.BURST;
                break;
            default:
                type = FireworkEffect.Type.BALL;
                break;
        }
        return FireworkEffect.builder().with(type).withColor(colorsOf(data.get("colors")))
            .withFade(colorsOf(data.get("fade_colors"))).trail(Nbt.flag(data, "has_trail", false))
            .flicker(Nbt.flag(data, "has_twinkle", false)).build();
    }

    public static Map<String, Object> effect(PotionEffect effect) {
        Map<String, Object> out = Nbt.compound();
        out.put("id", effect.getType().getKey().toString());
        out.put("amplifier", (byte) effect.getAmplifier());
        out.put("duration", effect.getDuration());
        out.put("ambient", Nbt.bool(effect.isAmbient()));
        out.put("show_particles", Nbt.bool(effect.hasParticles()));
        out.put("show_icon", Nbt.bool(effect.hasIcon()));
        if (effect.getHiddenPotionEffect() != null) {
            out.put("hidden_effect", effect(effect.getHiddenPotionEffect()));
        }
        return out;
    }

    public static PotionEffect effect(Map<String, Object> data) {
        NamespacedKey key = NamespacedKey.fromString(Nbt.string(data, "id", ""));
        PotionEffectType type = key == null ? null : PotionEffectType.getByKey(key);
        if (type == null) {
            return null;
        }
        Map<String, Object> hidden = Nbt.map(data.get("hidden_effect"));
        return new PotionEffect(type, Nbt.integer(data, "duration", 0), Nbt.integer(data, "amplifier", 0),
            Nbt.flag(data, "ambient", false), Nbt.flag(data, "show_particles", true), Nbt.flag(data, "show_icon", true),
            hidden == null ? null : effect(hidden));
    }

    public static List<PotionEffect> effects(Object list) {
        List<PotionEffect> out = new ArrayList<>();
        List<Object> entries = Nbt.list(list);
        if (entries != null) {
            for (Object entry : entries) {
                Map<String, Object> data = Nbt.map(entry);
                PotionEffect effect = data == null ? null : effect(data);
                if (effect != null) {
                    out.add(effect);
                }
            }
        }
        return out;
    }

    public static DyeColor dye(String name) {
        try {
            return DyeColor.valueOf(name.toUpperCase(java.util.Locale.ROOT));
        } catch (IllegalArgumentException e) {
            return null;
        }
    }

    public static String dye(DyeColor color) {
        return color.name().toLowerCase(java.util.Locale.ROOT);
    }

    public static Map<String, Object> stack(org.bukkit.inventory.ItemStack stack) {
        Map<String, Object> out = Nbt.compound();
        out.put("id", stack.getType().getKey().toString());
        out.put("count", stack.getAmount());
        Map<String, Object> components = org.basaltmc.bukkit.Items.componentsOf(stack);
        if (!components.isEmpty()) {
            out.put("components", Nbt.deepCopy(components));
        }
        return out;
    }

    public static org.bukkit.inventory.ItemStack stack(Map<String, Object> data) {
        org.bukkit.Material material = org.bukkit.Material.matchMaterial(Nbt.string(data, "id", ""));
        if (material == null || material.isAir()) {
            return null;
        }
        Map<String, Object> components = Nbt.map(data.get("components"));
        return new org.basaltmc.bukkit.BasaltItemStack(material, Nbt.integer(data, "count", 1),
            components == null ? Nbt.compound() : Nbt.deepCopy(components));
    }

    public static List<Object> stacks(List<org.bukkit.inventory.ItemStack> stacks) {
        List<Object> out = new ArrayList<>();
        for (org.bukkit.inventory.ItemStack stack : stacks) {
            if (stack != null && !stack.getType().isAir()) {
                out.add(stack(stack));
            }
        }
        return out;
    }

    public static List<org.bukkit.inventory.ItemStack> stacksOf(Object value) {
        List<org.bukkit.inventory.ItemStack> out = new ArrayList<>();
        List<Object> list = Nbt.list(value);
        if (list != null) {
            for (Object entry : list) {
                Map<String, Object> data = Nbt.map(entry);
                org.bukkit.inventory.ItemStack stack = data == null ? null : stack(data);
                if (stack != null) {
                    out.add(stack);
                }
            }
        }
        return out;
    }
}
