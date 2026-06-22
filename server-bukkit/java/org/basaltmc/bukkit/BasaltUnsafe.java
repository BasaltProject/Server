package org.basaltmc.bukkit;

import org.basaltmc.bukkit.gen.UnsafeValuesBase;
import org.basaltmc.bukkit.item.Nbt;

public final class BasaltUnsafe extends UnsafeValuesBase {
    static final BasaltUnsafe INSTANCE = new BasaltUnsafe();

    private static final int kDataVersion = 5023;

    @Override
    public int getDataVersion() {
        return kDataVersion;
    }

    @Override
    public boolean isSupportedApiVersion(String apiVersion) {
        return true;
    }

    @Override
    public String getMainLevelName() {
        return "world";
    }

    @Override
    public org.bukkit.potion.PotionType.InternalPotionData getInternalPotionData(org.bukkit.NamespacedKey key) {
        return org.basaltmc.bukkit.item.PotionData.of(key);
    }

    @Override
    public org.bukkit.Material getMaterial(String material, int version) {
        return org.bukkit.Material.matchMaterial(material);
    }

    @Override
    public org.bukkit.Material fromLegacy(org.bukkit.Material material) {
        return material;
    }

    @Override
    public org.bukkit.Material toLegacy(org.bukkit.Material material) {
        return material;
    }

    private static final int kProtocol = 777;

    @Override
    public int getProtocolVersion() {
        return kProtocol;
    }

    @Override
    public byte[] processClass(org.bukkit.plugin.PluginDescriptionFile plugin, String path, byte[] clazz) {
        return clazz;
    }

    @Override
    public void checkSupported(org.bukkit.plugin.PluginDescriptionFile plugin)
            throws org.bukkit.plugin.InvalidPluginException {}

    @Override
    public <B extends org.bukkit.Keyed> B get(io.papermc.paper.registry.RegistryKey<B> key,
            org.bukkit.NamespacedKey value) {
        return io.papermc.paper.registry.RegistryAccess.registryAccess().getRegistry(key).get(value);
    }

    @Override
    public org.bukkit.inventory.ItemStack modifyItemStack(org.bukkit.inventory.ItemStack stack, String arguments) {
        org.bukkit.inventory.ItemStack parsed = ItemParser.parse(stack.getType().getKey() + arguments);
        org.bukkit.inventory.ItemStack out = stack.clone();
        java.util.Map<String, Object> components = Items.componentsOf(out);
        components.putAll(Items.componentsOf(parsed));
        if (out instanceof BasaltItemStack own) {
            own.changed();
        }
        return out;
    }

    @Override
    public byte[] serializeEntity(org.bukkit.entity.Entity entity, io.papermc.paper.entity.EntitySerializationFlag... flags) {
        if (!(entity instanceof org.basaltmc.bukkit.entity.Cored cored)) {
            throw new IllegalArgumentException("Not an entity of this server");
        }
        byte[] named = cored.core().nbt("");
        if (named == null) {
            throw new IllegalArgumentException("The entity has no data to save");
        }
        try {
            java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
            try (java.util.zip.GZIPOutputStream zip = new java.util.zip.GZIPOutputStream(bytes)) {
                zip.write(named);
            }
            return bytes.toByteArray();
        } catch (java.io.IOException failure) {
            throw new IllegalStateException(failure);
        }
    }

    @Override
    public org.bukkit.entity.Entity deserializeEntity(byte[] data, org.bukkit.World world, boolean preserveUuid,
            boolean preservePassengers) {
        if (!(world instanceof BasaltWorld basalt)) {
            throw new IllegalArgumentException("Not a world of this server");
        }
        byte[] named;
        try (java.util.zip.GZIPInputStream zip = new java.util.zip.GZIPInputStream(
                new java.io.ByteArrayInputStream(data))) {
            named = zip.readAllBytes();
        } catch (java.io.IOException failure) {
            throw new IllegalArgumentException("Not a serialized entity", failure);
        }
        net.minecraft.nbt.CompoundTag tag = org.basaltmc.bukkit.facade.NbtCodec.fromBytes(named);
        if (tag == null) {
            throw new IllegalArgumentException("Not a serialized entity");
        }
        if (!preserveUuid) {
            tag.remove("UUID");
        }
        if (!preservePassengers) {
            tag.remove("Passengers");
        }
        Blocks.flushAll();
        int id = WorldNatives.spawnEntityNbt(basalt.getName(), org.basaltmc.bukkit.facade.NbtCodec.toBytes(tag),
            "default");
        if (id < 0) {
            throw new IllegalArgumentException("The server could not create the entity");
        }
        return basalt.entity(id);
    }

    @Override
    public org.bukkit.inventory.ItemStack deserializeStack(java.util.Map<String, Object> args) {
        if (!args.containsKey("id")) {
            return org.bukkit.inventory.ItemStack.deserialize(args);
        }
        java.util.Map<String, Object> root = Nbt.compound();
        root.put("id", String.valueOf(args.get("id")));
        root.put("count", args.get("count") instanceof Number count ? count.intValue() : 1);
        if (args.get("components") instanceof java.util.Map<?, ?> components) {
            root.put("components", org.basaltmc.bukkit.item.Nbt.copy(components));
        }
        org.bukkit.inventory.ItemStack stack = Items.fromBytes(Nbt.write(root));
        if (stack == null) {
            throw new IllegalArgumentException("Not an item: " + args);
        }
        return stack;
    }

    @Override
    public org.bukkit.inventory.ItemStack deserializeItemHover(net.kyori.adventure.text.event.HoverEvent.ShowItem hover) {
        StringBuilder text = new StringBuilder(hover.item().asString());
        boolean first = true;
        for (java.util.Map.Entry<net.kyori.adventure.key.Key, net.kyori.adventure.text.event.DataComponentValue> entry
                : hover.dataComponents().entrySet()) {
            if (entry.getValue() instanceof net.kyori.adventure.text.event.DataComponentValue.TagSerializable tagged) {
                text.append(first ? '[' : ',').append(entry.getKey().asString()).append('=')
                    .append(tagged.asBinaryTag().string());
                first = false;
            }
        }
        if (!first) {
            text.append(']');
        }
        org.bukkit.inventory.ItemStack stack = ItemParser.parse(text.toString());
        stack.setAmount(hover.count());
        return stack;
    }
}
