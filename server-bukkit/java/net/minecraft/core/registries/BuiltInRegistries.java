package net.minecraft.core.registries;

import net.minecraft.core.DefaultedMappedRegistry;
import net.minecraft.core.DefaultedRegistry;
import net.minecraft.core.MappedRegistry;
import net.minecraft.core.Registry;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.material.Fluids;
import org.basaltmc.bukkit.facade.RegistryTags;
import org.basaltmc.bukkit.Registries;
import org.basaltmc.bukkit.WorldNatives;

public class BuiltInRegistries {
    public static final DefaultedRegistry<Block> BLOCK = blocks();
    public static final DefaultedRegistry<Item> ITEM = items();
    public static final DefaultedRegistry<Fluid> FLUID = fluids();
    public static final DefaultedRegistry<EntityType<?>> ENTITY_TYPE = entityTypes();
    public static final Registry<BlockEntityType<?>> BLOCK_ENTITY_TYPE =
        new MappedRegistry<>(net.minecraft.core.registries.Registries.BLOCK_ENTITY_TYPE);

    private BuiltInRegistries() {}

    private static DefaultedRegistry<Block> blocks() {
        DefaultedMappedRegistry<Block> registry =
            new DefaultedMappedRegistry<>("minecraft:air", net.minecraft.core.registries.Registries.BLOCK);
        for (int i = 0; i < Block.blockCount(); i++) {
            Block block = Block.byIndex(i);
            registry.register(block.blockKey(), block);
        }
        registry.tagsFrom(() -> RegistryTags.bind(registry, "blocks"));
        return registry;
    }

    private static DefaultedRegistry<Item> items() {
        DefaultedMappedRegistry<Item> registry =
            new DefaultedMappedRegistry<>("minecraft:air", net.minecraft.core.registries.Registries.ITEM);
        for (String id : Registries.itemIds()) {
            Identifier key = Identifier.parse(id);
            registry.register(key, new Item(key));
        }
        registry.tagsFrom(() -> RegistryTags.bind(registry, "items"));
        return registry;
    }

    private static DefaultedRegistry<Fluid> fluids() {
        DefaultedMappedRegistry<Fluid> registry =
            new DefaultedMappedRegistry<>("minecraft:empty", net.minecraft.core.registries.Registries.FLUID);
        registry.register(Identifier.withDefaultNamespace("empty"), Fluids.EMPTY);
        registry.register(Identifier.withDefaultNamespace("flowing_water"), Fluids.FLOWING_WATER);
        registry.register(Identifier.withDefaultNamespace("water"), Fluids.WATER);
        registry.register(Identifier.withDefaultNamespace("flowing_lava"), Fluids.FLOWING_LAVA);
        registry.register(Identifier.withDefaultNamespace("lava"), Fluids.LAVA);
        return registry;
    }

    private static DefaultedRegistry<EntityType<?>> entityTypes() {
        DefaultedMappedRegistry<EntityType<?>> registry =
            new DefaultedMappedRegistry<>("minecraft:pig", net.minecraft.core.registries.Registries.ENTITY_TYPE);
        for (String id : WorldNatives.entityTypes()) {
            Identifier key = Identifier.parse(id);
            registry.register(key, new EntityType<>(key));
        }
        registry.tagsFrom(() -> RegistryTags.bind(registry, "entity_types"));
        return registry;
    }

}
