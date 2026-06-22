package org.basaltmc.bukkit;

import java.util.Locale;
import java.util.Map;
import java.util.function.UnaryOperator;
import net.kyori.adventure.key.Key;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.event.HoverEvent;
import net.kyori.adventure.text.format.NamedTextColor;
import net.kyori.adventure.text.format.TextColor;
import org.basaltmc.bukkit.gen.ItemFactoryBase;
import org.basaltmc.bukkit.item.ItemMetas;
import org.basaltmc.bukkit.item.MetaItem;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Color;
import org.bukkit.Material;
import org.bukkit.entity.EntityType;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.meta.ItemMeta;

public final class BasaltItemFactory extends ItemFactoryBase {
    static final BasaltItemFactory INSTANCE = new BasaltItemFactory();

    private BasaltItemFactory() {
        ItemMetas.register();
    }

    @Override
    public ItemMeta getItemMeta(Material material) {
        if (material == null) {
            throw new IllegalArgumentException("Material cannot be null");
        }
        if (material.isAir() || !material.isItem()) {
            return null;
        }
        return ItemMetas.create(material, Nbt.compound());
    }

    @Override
    public boolean isApplicable(ItemMeta meta, ItemStack stack) {
        return stack != null && isApplicable(meta, stack.getType());
    }

    @Override
    public boolean isApplicable(ItemMeta meta, Material material) {
        if (meta == null || material == null || material.isAir()) {
            return false;
        }
        if (!(meta instanceof MetaItem own)) {
            throw new IllegalArgumentException("Meta of " + meta.getClass().toString() + " not created by " + getClass().getName());
        }
        return own.applicableTo(material);
    }

    @Override
    public boolean equals(ItemMeta meta1, ItemMeta meta2) {
        if (meta1 == meta2) {
            return true;
        }
        if (meta1 != null && !(meta1 instanceof MetaItem) || meta2 != null && !(meta2 instanceof MetaItem)) {
            throw new IllegalArgumentException("Meta not created by this factory");
        }
        Map<String, Object> left = meta1 == null ? Nbt.compound() : ((MetaItem) meta1).components();
        Map<String, Object> right = meta2 == null ? Nbt.compound() : ((MetaItem) meta2).components();
        return Nbt.same(left, right);
    }

    @Override
    public ItemMeta asMetaFor(ItemMeta meta, ItemStack stack) {
        return asMetaFor(meta, stack.getType());
    }

    @Override
    public ItemMeta asMetaFor(ItemMeta meta, Material material) {
        if (!(meta instanceof MetaItem own)) {
            throw new IllegalArgumentException("Meta of " + (meta == null ? "null" : meta.getClass().toString())
                + " not created by " + getClass().getName());
        }
        if (own.applicableTo(material)) {
            return own.clone();
        }
        return ItemMetas.create(material, Nbt.deepCopy(own.components()));
    }

    @Override
    public Color getDefaultLeatherColor() {
        return Color.fromRGB(0xA06540);
    }

    @Override
    public ItemStack createItemStack(String input) {
        return ItemParser.parse(input);
    }

    @Override
    public Material getSpawnEgg(EntityType type) {
        return type == null ? null : Material.matchMaterial(type.getKey().getKey() + "_spawn_egg");
    }

    @Override
    public ItemStack enchantItem(org.bukkit.entity.Entity entity, ItemStack item, int level, boolean allowTreasures) {
        return enchantItem(item, level, allowTreasures);
    }

    @Override
    public ItemStack enchantItem(org.bukkit.World world, ItemStack item, int level, boolean allowTreasures) {
        return enchantItem(item, level, allowTreasures);
    }

    @Override
    public ItemStack enchantItem(ItemStack item, int level, boolean allowTreasures) {
        return enchantWithLevels(item, level, allowTreasures, new java.util.Random());
    }

    @Override
    public ItemStack enchantWithLevels(ItemStack item, int levels, boolean allowTreasures, java.util.Random random) {
        return enchantRolled(item, levels, allowTreasures ? "#minecraft:on_random_loot" : "#minecraft:in_enchanting_table",
            random);
    }

    @Override
    public ItemStack enchantWithLevels(ItemStack item, int levels,
            io.papermc.paper.registry.set.RegistryKeySet<org.bukkit.enchantments.Enchantment> options,
            java.util.Random random) {
        if (options instanceof io.papermc.paper.registry.tag.Tag<?> tag) {
            return enchantRolled(item, levels, "#" + tag.tagKey().key().asString(), random);
        }
        StringBuilder ids = new StringBuilder();
        for (var key : options.values()) {
            ids.append(ids.length() == 0 ? "" : ",").append(key.key().asString());
        }
        return enchantRolled(item, levels, ids.toString(), random);
    }

    private static ItemStack enchantRolled(ItemStack item, int levels, String options, java.util.Random random) {
        if (item.getType().isAir()) {
            throw new IllegalArgumentException("Cannot enchant an empty stack");
        }
        String[] made = LeftoverNatives.enchantItem(new String[] {item.getType().getKey().toString(),
            String.valueOf(item.getAmount()), ItemMetas.snbt(Items.componentsOf(item))},
            org.bukkit.Bukkit.getWorlds().get(0).getName(), options, levels, random.nextLong());
        if (made == null) {
            throw new IllegalStateException("The server could not roll enchantments");
        }
        @SuppressWarnings("unchecked")
        Map<String, Object> components = made[2].isEmpty() ? Nbt.compound() : (Map<String, Object>) ItemParser.value(made[2]);
        return new BasaltItemStack(item.getType(), item.getAmount(), components);
    }

    @Override
    public HoverEvent<HoverEvent.ShowItem> asHoverEvent(ItemStack item, UnaryOperator<HoverEvent.ShowItem> op) {
        return HoverEvent.showItem(op.apply(HoverEvent.ShowItem.showItem(Key.key(item.getType().getKey().toString()),
            item.getAmount())));
    }

    @Override
    public Component displayName(ItemStack stack) {
        Component name = stack.effectiveName();
        TextColor color = switch (rarity(stack)) {
            case "uncommon" -> NamedTextColor.YELLOW;
            case "rare" -> NamedTextColor.AQUA;
            case "epic" -> NamedTextColor.LIGHT_PURPLE;
            default -> NamedTextColor.WHITE;
        };
        return Component.text().append(Component.text("["), name, Component.text("]")).color(color)
            .hoverEvent(asHoverEvent(stack, UnaryOperator.identity())).build();
    }

    private static String rarity(ItemStack stack) {
        return Nbt.string(Items.componentsOf(stack), "minecraft:rarity", "common").toLowerCase(Locale.ROOT);
    }

    @Override
    public String getI18NDisplayName(ItemStack item) {
        return null;
    }

    @Override
    public ItemStack ensureServerConversions(ItemStack item) {
        return item;
    }

    @Override
    public net.md_5.bungee.api.chat.hover.content.Content hoverContentOf(ItemStack stack) {
        return new net.md_5.bungee.api.chat.hover.content.Item(stack.getType().getKey().toString(), stack.getAmount(),
            net.md_5.bungee.api.chat.ItemTag.ofNbt(ItemMetas.snbt(Items.componentsOf(stack))));
    }

    @Override
    public net.md_5.bungee.api.chat.hover.content.Content hoverContentOf(org.bukkit.entity.Entity entity) {
        return hoverContentOf(entity, (net.md_5.bungee.api.chat.BaseComponent[]) null);
    }

    @Override
    public net.md_5.bungee.api.chat.hover.content.Content hoverContentOf(org.bukkit.entity.Entity entity, String name) {
        return hoverContentOf(entity, name == null ? null : new net.md_5.bungee.api.chat.BaseComponent[] {
            new net.md_5.bungee.api.chat.TextComponent(name)});
    }

    @Override
    public net.md_5.bungee.api.chat.hover.content.Content hoverContentOf(org.bukkit.entity.Entity entity,
            net.md_5.bungee.api.chat.BaseComponent name) {
        return hoverContentOf(entity, name == null ? null : new net.md_5.bungee.api.chat.BaseComponent[] {name});
    }

    @Override
    public net.md_5.bungee.api.chat.hover.content.Content hoverContentOf(org.bukkit.entity.Entity entity,
            net.md_5.bungee.api.chat.BaseComponent[] name) {
        return new net.md_5.bungee.api.chat.hover.content.Entity(entity.getType().getKey().toString(),
            entity.getUniqueId().toString(), name == null || name.length == 0 ? null : name[0]);
    }
}
