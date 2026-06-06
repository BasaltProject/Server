package org.basaltmc.bukkit.fawe;

import com.fastasyncworldedit.core.queue.IChunkGet;
import com.sk89q.jnbt.Tag;
import com.sk89q.worldedit.blocks.BaseItem;
import com.sk89q.worldedit.blocks.BaseItemStack;
import com.sk89q.worldedit.bukkit.adapter.BukkitImplAdapter;
import com.sk89q.worldedit.entity.BaseEntity;
import com.sk89q.worldedit.internal.wna.WorldNativeAccess;
import com.sk89q.worldedit.math.BlockVector3;
import com.sk89q.worldedit.registry.state.BooleanProperty;
import com.sk89q.worldedit.registry.state.DirectionalProperty;
import com.sk89q.worldedit.registry.state.EnumProperty;
import com.sk89q.worldedit.registry.state.IntegerProperty;
import com.sk89q.worldedit.registry.state.Property;
import com.sk89q.worldedit.util.Direction;
import com.sk89q.worldedit.util.SideEffect;
import com.sk89q.worldedit.util.formatting.text.Component;
import com.sk89q.worldedit.util.formatting.text.TextComponent;
import com.sk89q.worldedit.world.DataFixer;
import com.sk89q.worldedit.world.biome.BiomeType;
import com.sk89q.worldedit.world.block.BaseBlock;
import com.sk89q.worldedit.world.block.BlockState;
import com.sk89q.worldedit.world.block.BlockType;
import com.sk89q.worldedit.world.item.ItemType;
import com.sk89q.worldedit.world.registry.BlockMaterial;
import java.util.ArrayList;
import java.util.Collection;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.OptionalInt;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import org.basaltmc.bukkit.BasaltBlockData;
import org.basaltmc.bukkit.Registries;
import org.basaltmc.bukkit.WorldNatives;
import org.bukkit.Location;
import org.bukkit.NamespacedKey;
import org.bukkit.World;
import org.bukkit.block.TileState;
import org.bukkit.block.data.BlockData;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Player;
import org.bukkit.inventory.ItemStack;
import org.enginehub.linbus.tree.LinCompoundTag;

/** FastAsyncWorldEdit's view of the server, answered by Basalt's own world services instead of Minecraft internals. */
public final class BasaltFaweAdapter implements BukkitImplAdapter<Object> {
    private final Map<String, Map<String, ? extends Property<?>>> properties = new ConcurrentHashMap<>();
    private final Map<String, BlockMaterial> materials = new ConcurrentHashMap<>();
    private volatile Map<String, List<Property<?>>> allProperties;

    public BasaltFaweAdapter() {
        org.bukkit.plugin.Plugin plugin = org.bukkit.Bukkit.getPluginManager().getPlugin("FastAsyncWorldEdit");
        org.bukkit.Bukkit.getScheduler().runTask(plugin, () -> com.fastasyncworldedit.core.Fawe.instance().setMainThread());
    }

    @Override
    public DataFixer getDataFixer() {
        return new DataFixer() {
            @Override
            public <T> T fixUp(FixType<T> type, T original, int srcVersion) {
                return original;
            }
        };
    }

    @Override
    public BlockState getBlock(Location location) {
        int state = WorldNatives.state(location.getWorld().getName(), location.getBlockX(), location.getBlockY(),
            location.getBlockZ(), true);
        return com.sk89q.worldedit.world.block.BlockTypesCache.states[StateTable.toOrdinal(state)];
    }

    @Override
    public BaseBlock getFullBlock(Location location) {
        return getBlock(location).toBaseBlock();
    }

    @Override
    public WorldNativeAccess<?, ?, ?> createWorldNativeAccess(World world) {
        return new BasaltNativeAccess(world.getName());
    }

    @Override
    public BaseEntity getEntity(Entity entity) {
        return null;
    }

    @Override
    public Entity createEntity(Location location, BaseEntity state) {
        return null;
    }

    @Override
    public Component getRichBlockName(BlockType blockType) {
        return TextComponent.of(blockType.id());
    }

    @Override
    public Component getRichItemName(ItemType itemType) {
        return TextComponent.of(itemType.id());
    }

    @Override
    public Component getRichItemName(BaseItemStack itemStack) {
        return TextComponent.of(itemStack.getType().id());
    }

    @Override
    public Map<String, ? extends Property<?>> getProperties(BlockType blockType) {
        return properties.computeIfAbsent(blockType.id(), id -> propertiesOf(id));
    }

    private static Map<String, ? extends Property<?>> propertiesOf(String id) {
        int block = Registries.blockIndex(id);
        Map<String, Property<?>> out = new LinkedHashMap<>();
        if (block < 0) {
            return out;
        }
        Registries.Properties facts = Registries.propertiesOf(block);
        for (int i = 0; i < facts.names().length; i++) {
            out.put(facts.names()[i], property(facts.names()[i], facts.values()[i]));
        }
        return out;
    }

    private static Property<?> property(String name, String[] values) {
        boolean bool = values.length == 2 && (values[0].equals("true") || values[0].equals("false"))
            && (values[1].equals("true") || values[1].equals("false"));
        if (bool) {
            List<Boolean> list = new ArrayList<>();
            for (String value : values) {
                list.add(Boolean.parseBoolean(value));
            }
            return new BooleanProperty(name, list);
        }
        boolean numeric = true;
        for (String value : values) {
            try {
                Integer.parseInt(value);
            } catch (NumberFormatException e) {
                numeric = false;
                break;
            }
        }
        if (numeric) {
            List<Integer> list = new ArrayList<>();
            for (String value : values) {
                list.add(Integer.parseInt(value));
            }
            return new IntegerProperty(name, list);
        }
        if (name.equals("facing")) {
            List<Direction> list = new ArrayList<>();
            for (String value : values) {
                try {
                    list.add(Direction.valueOf(value.toUpperCase(Locale.ROOT)));
                } catch (IllegalArgumentException e) {
                    list = null;
                    break;
                }
            }
            if (list != null) {
                return new DirectionalProperty(name, list);
            }
        }
        return new EnumProperty(name, new ArrayList<>(List.of(values)));
    }

    @Override
    public void sendFakeNBT(Player player, BlockVector3 pos, LinCompoundTag nbtData) {}

    @Override
    public void sendFakeNBT(Player player, BlockVector3 pos, TileState tile, LinCompoundTag nbtData) {}

    @Override
    public void sendFakeOP(Player player) {}

    @Override
    public boolean canPlaceAt(World world, BlockVector3 position, BlockState blockState) {
        return true;
    }

    @Override
    public ItemStack adapt(BaseItemStack item) {
        String id = item.getType().id();
        org.bukkit.Material material = org.bukkit.Material.getMaterial(
            id.substring(id.indexOf(':') + 1).toUpperCase(Locale.ROOT));
        return new ItemStack(material == null ? org.bukkit.Material.AIR : material, item.getAmount());
    }

    @Override
    public BaseItemStack adapt(ItemStack itemStack) {
        return new BaseItemStack(com.sk89q.worldedit.world.item.ItemTypes.get(itemStack.getType().getKey().toString()),
            itemStack.getAmount());
    }

    @Override
    public Set<SideEffect> getSupportedSideEffects() {
        return EnumSet.of(SideEffect.NETWORK, SideEffect.LIGHTING);
    }

    @Override
    public OptionalInt getInternalBlockStateId(BlockData data) {
        return OptionalInt.of(BasaltBlockData.stateOf(data));
    }

    @Override
    public OptionalInt getInternalBlockStateId(BlockState state) {
        return OptionalInt.of(StateTable.toState(state.getOrdinal()));
    }

    @Override
    public BlockMaterial getMaterial(BlockType blockType) {
        return materials.computeIfAbsent(blockType.id(), id -> {
            int block = Registries.blockIndex(id);
            return new BasaltBlockMaterial(id, block < 0 ? -1 : Registries.defaultState(block));
        });
    }

    @Override
    public BlockMaterial getMaterial(BlockState state) {
        return getMaterial(state.getBlockType());
    }

    @Override
    public Collection<String> getRegisteredDefaultBlockStates() {
        List<String> out = new ArrayList<>();
        for (int block = 0; block < Registries.blockCount(); block++) {
            Registries.Properties facts = Registries.propertiesOf(block);
            out.add(facts.names().length == 0 ? Registries.blockName(block)
                : WorldNatives.stateText(Registries.defaultState(block)));
        }
        return out;
    }

    @Override
    public Map<String, List<Property<?>>> getAllProperties() {
        Map<String, List<Property<?>>> built = allProperties;
        if (built != null) {
            return built;
        }
        Map<String, List<Property<?>>> out = new HashMap<>();
        Map<String, Set<String>> seen = new HashMap<>();
        for (int block = 0; block < Registries.blockCount(); block++) {
            Registries.Properties facts = Registries.propertiesOf(block);
            for (int i = 0; i < facts.names().length; i++) {
                String signature = String.join("|", facts.values()[i]);
                if (seen.computeIfAbsent(facts.names()[i], k -> new java.util.HashSet<>()).add(signature)) {
                    out.computeIfAbsent(facts.names()[i], k -> new ArrayList<>())
                        .add(property(facts.names()[i], facts.values()[i]));
                }
            }
        }
        allProperties = out;
        return out;
    }

    @Override
    public Iterable<NamespacedKey> getRegisteredBiomes() {
        List<NamespacedKey> out = new ArrayList<>();
        for (String id : Registries.biomeIds()) {
            out.add(NamespacedKey.fromString(id));
        }
        return out;
    }

    @Override
    public int getInternalBiomeId(BiomeType biome) {
        String[] ids = Registries.biomeIds();
        for (int i = 0; i < ids.length; i++) {
            if (ids[i].equals(biome.id())) {
                return i;
            }
        }
        return 0;
    }

    @Override
    public BiomeType getBiome(Location location) {
        String id = WorldNatives.biome(location.getWorld().getName(), location.getBlockX(), location.getBlockY(),
            location.getBlockZ());
        return id == null ? null : BiomeType.REGISTRY.get(id);
    }

    @Override
    public void setBiome(Location location, BiomeType biome) {
        WorldNatives.setBiome(location.getWorld().getName(), location.getBlockX(), location.getBlockY(),
            location.getBlockZ(), biome.id());
    }

    @Override
    public IChunkGet get(World world, int chunkX, int chunkZ) {
        return new BasaltGetBlocks(world.getName(), chunkX, chunkZ, world.getMinHeight(), world.getMaxHeight() - 1);
    }

    @Override
    public com.fastasyncworldedit.core.extent.processor.lighting.RelighterFactory getRelighterFactory() {
        return (mode, world, queue) -> new BasaltRelighter();
    }

    @Override
    public Object fromNative(Tag foreign) {
        return foreign;
    }

    @Override
    public Tag toNative(Object foreign) {
        return (Tag) foreign;
    }
}
