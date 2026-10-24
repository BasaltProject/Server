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

    private static org.bukkit.Material modernOf(org.bukkit.Material material, byte data) {
        if (!material.isLegacy()) {
            return material;
        }
        org.bukkit.Material modern = org.bukkit.Material.getMaterial(material.name().substring("LEGACY_".length()));
        if (modern == null || data != 0) {
            throw Unsupported.hit("UnsafeValues#fromLegacy of " + material + " with data " + data);
        }
        return modern;
    }

    @Override
    public org.bukkit.block.data.BlockData fromLegacy(org.bukkit.Material material, byte data) {
        return org.bukkit.Bukkit.createBlockData(modernOf(material, data));
    }

    @Override
    public org.bukkit.Material fromLegacy(org.bukkit.material.MaterialData data) {
        return modernOf(data.getItemType(), data.getData());
    }

    @Override
    public org.bukkit.Material fromLegacy(org.bukkit.material.MaterialData data, boolean itemPriority) {
        return modernOf(data.getItemType(), data.getData());
    }

    @Override
    public com.google.gson.JsonObject serializeItemAsJson(org.bukkit.inventory.ItemStack stack) {
        if (stack == null || stack.getType().isAir()) {
            throw new IllegalArgumentException("Cannot serialize an empty stack");
        }
        return org.basaltmc.bukkit.item.ItemJson.write(stack, kDataVersion);
    }

    @Override
    public org.bukkit.inventory.ItemStack deserializeItemFromJson(com.google.gson.JsonObject json) {
        return org.basaltmc.bukkit.item.ItemJson.read(json);
    }

    @Override
    public int nextEntityId(org.bukkit.World world) {
        return RestNatives.nextEntityId();
    }

    private static final String kPackMeta = "{\"pack\":{\"description\":\"Data pack for resources provided by "
        + "Bukkit plugins\",\"min_format\":121,\"max_format\":121}}";

    private static java.io.File advancementFile(String namespace, String path) {
        java.io.File pack = new java.io.File(new java.io.File(org.bukkit.Bukkit.getWorlds().get(0).getWorldFolder(),
            "datapacks"), "bukkit");
        return new java.io.File(new java.io.File(pack, "data/" + namespace + "/advancement"), path + ".json");
    }

    @Override
    public org.bukkit.advancement.Advancement loadAdvancement(net.kyori.adventure.key.Key key, String advancement,
            boolean persist) {
        java.io.File file = advancementFile(key.namespace(), key.value());
        try {
            java.io.File meta = new java.io.File(file.getParentFile().getParentFile().getParentFile().getParentFile(),
                "pack.mcmeta");
            java.nio.file.Files.createDirectories(file.getParentFile().toPath());
            if (!meta.exists()) {
                java.nio.file.Files.writeString(meta.toPath(), kPackMeta);
            }
            java.nio.file.Files.writeString(file.toPath(), advancement);
        } catch (java.io.IOException e) {
            return null;
        }
        boolean reloaded = RestNatives.reloadData();
        org.bukkit.advancement.Advancement loaded = reloaded
            ? org.bukkit.Bukkit.getAdvancement(new org.bukkit.NamespacedKey(key.namespace(), key.value())) : null;
        if (!persist) {
            file.delete();
        }
        return loaded;
    }

    @Override
    public java.util.List<org.bukkit.advancement.Advancement> loadAdvancements(
            java.util.Map<net.kyori.adventure.key.Key, String> advancements, boolean persist) {
        java.util.List<org.bukkit.advancement.Advancement> loaded = new java.util.ArrayList<>();
        for (java.util.Map.Entry<net.kyori.adventure.key.Key, String> entry : advancements.entrySet()) {
            org.bukkit.advancement.Advancement one = loadAdvancement(entry.getKey(), entry.getValue(), persist);
            if (one != null) {
                loaded.add(one);
            }
        }
        return loaded;
    }

    @Override
    public boolean removeAdvancement(org.bukkit.NamespacedKey key) {
        return advancementFile(key.getNamespace(), key.getKey()).delete();
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
