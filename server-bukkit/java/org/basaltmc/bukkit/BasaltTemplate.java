package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Random;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.persistence.MemoryPdcStore;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.RegionAccessor;
import org.bukkit.World;
import org.bukkit.block.BlockState;
import org.bukkit.block.data.BlockData;
import org.bukkit.block.structure.Mirror;
import org.bukkit.block.structure.StructureRotation;
import org.bukkit.craftbukkit.persistence.CraftPersistentDataContainer;
import org.bukkit.entity.Entity;
import org.bukkit.generator.LimitedRegion;
import org.bukkit.persistence.PersistentDataContainer;
import org.bukkit.structure.Palette;
import org.bukkit.structure.Structure;
import org.bukkit.util.BlockTransformer;
import org.bukkit.util.BlockVector;
import org.bukkit.util.EntityTransformer;

/** A structure template held as the compound of a structure file; placing and capturing go through the server. */
final class BasaltTemplate implements Structure {
    private static final String kValues = "BukkitValues";

    private Map<String, Object> data;
    private CraftPersistentDataContainer container;
    private MemoryPdcStore store;

    BasaltTemplate(Map<String, Object> data) {
        this.data = data;
    }

    static BasaltTemplate empty() {
        Map<String, Object> data = Nbt.compound();
        List<Object> size = new ArrayList<>();
        size.add(0);
        size.add(0);
        size.add(0);
        data.put("size", size);
        data.put("palette", new ArrayList<Object>());
        data.put("blocks", new ArrayList<Object>());
        data.put("entities", new ArrayList<Object>());
        return new BasaltTemplate(data);
    }

    Map<String, Object> data() {
        flushValues();
        return data;
    }

    byte[] bytes() {
        return Nbt.write(data());
    }

    private void flushValues() {
        if (store != null) {
            Map<String, Object> values = Nbt.readNamed(NbtCodec.toBytes(store.load()));
            if (values == null || values.isEmpty()) {
                data.remove(kValues);
            } else {
                data.put(kValues, values);
            }
        }
    }

    @Override
    public PersistentDataContainer getPersistentDataContainer() {
        if (container == null) {
            Map<String, Object> held = Nbt.map(data.get(kValues));
            net.minecraft.nbt.CompoundTag tag = held == null ? new net.minecraft.nbt.CompoundTag()
                : NbtCodec.fromBytes(Nbt.writeNamed(held));
            store = new MemoryPdcStore(tag);
            container = new CraftPersistentDataContainer(store);
        }
        return container;
    }

    @Override
    public BlockVector getSize() {
        List<Object> size = Nbt.list(data.get("size"));
        if (size == null || size.size() < 3) {
            return new BlockVector(0, 0, 0);
        }
        return new BlockVector(((Number) size.get(0)).intValue(), ((Number) size.get(1)).intValue(),
            ((Number) size.get(2)).intValue());
    }

    private List<List<Object>> palettes() {
        List<List<Object>> out = new ArrayList<>();
        List<Object> many = Nbt.list(data.get("palettes"));
        if (many != null) {
            for (Object palette : many) {
                List<Object> entries = Nbt.list(palette);
                out.add(entries == null ? List.of() : entries);
            }
        } else {
            List<Object> single = Nbt.list(data.get("palette"));
            out.add(single == null ? List.of() : single);
        }
        return out;
    }

    private static BlockData blockData(Object entry) {
        String name;
        Map<String, Object> properties = null;
        Map<String, Object> map = Nbt.map(entry);
        if (map != null) {
            name = Nbt.string(map, "Name", Nbt.string(map, "", "minecraft:air"));
            properties = Nbt.map(map.get("Properties"));
        } else {
            name = String.valueOf(entry);
        }
        StringBuilder text = new StringBuilder(name);
        if (properties != null && !properties.isEmpty()) {
            text.append('[');
            boolean first = true;
            for (Map.Entry<String, Object> property : properties.entrySet()) {
                text.append(first ? "" : ",").append(property.getKey()).append('=').append(property.getValue());
                first = false;
            }
            text.append(']');
        }
        return Bukkit.createBlockData(text.toString());
    }

    private final class Layer implements Palette {
        private final List<Object> entries;

        Layer(List<Object> entries) {
            this.entries = entries;
        }

        @Override
        public List<BlockState> getBlocks() {
            List<BlockState> out = new ArrayList<>();
            List<Object> blocks = Nbt.list(data.get("blocks"));
            for (Object block : blocks == null ? List.of() : blocks) {
                Map<String, Object> map = Nbt.map(block);
                List<Object> pos = map == null ? null : Nbt.list(map.get("pos"));
                int state = map == null ? -1 : Nbt.integer(map, "state", -1);
                if (pos == null || pos.size() < 3 || state < 0 || state >= entries.size()) {
                    continue;
                }
                out.add(new BasaltBlockState(null, ((Number) pos.get(0)).intValue(), ((Number) pos.get(1)).intValue(),
                    ((Number) pos.get(2)).intValue(), blockData(entries.get(state))));
            }
            return out;
        }

        @Override
        public int getBlockCount() {
            List<Object> blocks = Nbt.list(data.get("blocks"));
            return blocks == null ? 0 : blocks.size();
        }
    }

    @Override
    public List<Palette> getPalettes() {
        List<Palette> out = new ArrayList<>();
        for (List<Object> entries : palettes()) {
            out.add(new Layer(entries));
        }
        return out;
    }

    @Override
    public int getPaletteCount() {
        return palettes().size();
    }

    @Override
    public List<Entity> getEntities() {
        List<Entity> out = new ArrayList<>();
        List<Object> saved = Nbt.list(data.get("entities"));
        if (saved == null || !(Bukkit.getWorlds().get(0) instanceof BasaltWorld world)) {
            return out;
        }
        for (Object value : saved) {
            Map<String, Object> entry = Nbt.map(value);
            Map<String, Object> nbt = entry == null ? null : Nbt.map(entry.get("nbt"));
            List<Object> pos = entry == null ? null : Nbt.list(entry.get("pos"));
            if (nbt == null || pos == null || pos.size() < 3) {
                continue;
            }
            org.bukkit.entity.EntityType type = org.basaltmc.bukkit.entity.EntityCore.typeOf(
                Nbt.string(nbt, "id", ""));
            if (type == org.bukkit.entity.EntityType.UNKNOWN) {
                continue;
            }
            List<Object> rotation = Nbt.list(nbt.get("Rotation"));
            Entity made = org.basaltmc.bukkit.entity.EntityCore.template(world, type, ((Number) pos.get(0)).doubleValue(),
                ((Number) pos.get(1)).doubleValue(), ((Number) pos.get(2)).doubleValue(),
                rotation == null || rotation.size() < 2 ? 0.0F : ((Number) rotation.get(0)).floatValue(),
                rotation == null || rotation.size() < 2 ? 0.0F : ((Number) rotation.get(1)).floatValue());
            ((org.basaltmc.bukkit.entity.Cored) made).core().seedNbt(Nbt.writeNamed(nbt));
            out.add(made);
        }
        return out;
    }

    @Override
    public int getEntityCount() {
        List<Object> entities = Nbt.list(data.get("entities"));
        return entities == null ? 0 : entities.size();
    }

    private void placeAt(World world, int x, int y, int z, boolean includeEntities, StructureRotation rotation,
            Mirror mirror, int palette, float integrity, Random random) {
        if (!(world instanceof BasaltWorld basalt)) {
            throw new IllegalArgumentException("A structure is placed in a world the server runs");
        }
        if (palette < -1 || palette >= getPaletteCount()) {
            throw new IllegalArgumentException("Palette index out of range");
        }
        if (integrity < 0 || integrity > 1) {
            throw new IllegalArgumentException("Integrity must be between 0 and 1");
        }
        Blocks.flushAll();
        boolean placed = LeftoverNatives.structurePlace(bytes(), basalt.getName(), x, y, z, rotation.ordinal(),
            mirror.ordinal(), integrity, random.nextLong(), includeEntities, palette);
        if (!placed) {
            throw new IllegalStateException("The server could not place the structure");
        }
    }

    private static void noTransformers(Collection<BlockTransformer> blocks, Collection<EntityTransformer> entities) {
        if (blocks != null && !blocks.isEmpty() || entities != null && !entities.isEmpty()) {
            throw Unsupported.hit("Structure#place with block or entity transformers");
        }
    }

    @Override
    public void place(Location location, boolean includeEntities, StructureRotation rotation, Mirror mirror,
            int palette, float integrity, Random random) {
        placeAt(location.getWorld(), location.getBlockX(), location.getBlockY(), location.getBlockZ(), includeEntities,
            rotation, mirror, palette, integrity, random);
    }

    @Override
    public void place(Location location, boolean includeEntities, StructureRotation rotation, Mirror mirror,
            int palette, float integrity, Random random, Collection<BlockTransformer> blockTransformers,
            Collection<EntityTransformer> entityTransformers) {
        noTransformers(blockTransformers, entityTransformers);
        place(location, includeEntities, rotation, mirror, palette, integrity, random);
    }

    @Override
    public void place(RegionAccessor regionAccessor, BlockVector location, boolean includeEntities,
            StructureRotation rotation, Mirror mirror, int palette, float integrity, Random random) {
        World world;
        if (regionAccessor instanceof LimitedRegion limited) {
            world = limited.getWorld();
            BlockVector size = getSize();
            int[][] corners = {{0, 0}, {size.getBlockX() - 1, 0}, {0, size.getBlockZ() - 1},
                {size.getBlockX() - 1, size.getBlockZ() - 1}};
            for (int[] corner : corners) {
                int x = mirror == Mirror.FRONT_BACK ? -corner[0] : corner[0];
                int z = mirror == Mirror.LEFT_RIGHT ? -corner[1] : corner[1];
                int[] turned = switch (rotation) {
                    case CLOCKWISE_90 -> new int[] {-z, x};
                    case CLOCKWISE_180 -> new int[] {-x, -z};
                    case COUNTERCLOCKWISE_90 -> new int[] {z, -x};
                    default -> new int[] {x, z};
                };
                if (!limited.isInRegion(location.getBlockX() + turned[0], location.getBlockY(),
                    location.getBlockZ() + turned[1])) {
                    throw new IllegalArgumentException("The structure does not fit in the region of chunk "
                        + limited.getCenterChunkX() + ", " + limited.getCenterChunkZ());
                }
            }
        } else if (regionAccessor instanceof World whole) {
            world = whole;
        } else {
            throw new IllegalArgumentException("A structure is placed in a world or a limited region");
        }
        placeAt(world, location.getBlockX(), location.getBlockY(), location.getBlockZ(), includeEntities, rotation,
            mirror, palette, integrity, random);
    }

    @Override
    public void place(RegionAccessor regionAccessor, BlockVector location, boolean includeEntities,
            StructureRotation rotation, Mirror mirror, int palette, float integrity, Random random,
            Collection<BlockTransformer> blockTransformers, Collection<EntityTransformer> entityTransformers) {
        noTransformers(blockTransformers, entityTransformers);
        place(regionAccessor, location, includeEntities, rotation, mirror, palette, integrity, random);
    }

    private void capture(World world, int x, int y, int z, int sizeX, int sizeY, int sizeZ, boolean includeEntities) {
        if (!(world instanceof BasaltWorld basalt)) {
            throw new IllegalArgumentException("A structure is filled from a world the server runs");
        }
        if (sizeX <= 0 || sizeY <= 0 || sizeZ <= 0) {
            throw new IllegalArgumentException("The structure size must be positive");
        }
        Blocks.flushAll();
        byte[] captured = LeftoverNatives.structureCapture(basalt.getName(), x, y, z, sizeX, sizeY, sizeZ,
            includeEntities);
        if (captured == null) {
            throw new IllegalStateException("The server could not read the area");
        }
        Map<String, Object> read = Nbt.read(captured);
        Object values = data.get(kValues);
        data = read;
        if (values != null) {
            data.put(kValues, values);
        }
    }

    @Override
    public void fill(Location corner1, Location corner2, boolean includeEntities) {
        int x = Math.min(corner1.getBlockX(), corner2.getBlockX());
        int y = Math.min(corner1.getBlockY(), corner2.getBlockY());
        int z = Math.min(corner1.getBlockZ(), corner2.getBlockZ());
        capture(corner1.getWorld(), x, y, z, Math.abs(corner1.getBlockX() - corner2.getBlockX()) + 1,
            Math.abs(corner1.getBlockY() - corner2.getBlockY()) + 1,
            Math.abs(corner1.getBlockZ() - corner2.getBlockZ()) + 1, includeEntities);
    }

    @Override
    public void fill(Location origin, BlockVector size, boolean includeEntities) {
        capture(origin.getWorld(), origin.getBlockX(), origin.getBlockY(), origin.getBlockZ(), size.getBlockX(),
            size.getBlockY(), size.getBlockZ(), includeEntities);
    }

    BasaltTemplate copy() {
        return new BasaltTemplate(Nbt.deepCopy(data()));
    }
}
