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
            Entity made = entityOf(world, Nbt.map(value), 0.0, 0.0, 0.0, null);
            if (made != null) {
                out.add(made);
            }
        }
        return out;
    }

    /** A detached entity from one saved entity of the structure, moved by the offset and turned when told to. */
    private static Entity entityOf(BasaltWorld world, Map<String, Object> entry, double offsetX, double offsetY,
            double offsetZ, Float yaw) {
        Map<String, Object> nbt = entry == null ? null : Nbt.map(entry.get("nbt"));
        List<Object> pos = entry == null ? null : Nbt.list(entry.get("pos"));
        if (nbt == null || pos == null || pos.size() < 3) {
            return null;
        }
        org.bukkit.entity.EntityType type = org.basaltmc.bukkit.entity.EntityCore.typeOf(Nbt.string(nbt, "id", ""));
        if (type == org.bukkit.entity.EntityType.UNKNOWN) {
            return null;
        }
        List<Object> rotation = Nbt.list(nbt.get("Rotation"));
        boolean rotated = rotation != null && rotation.size() >= 2;
        Entity made = org.basaltmc.bukkit.entity.EntityCore.template(world, type,
            offsetX + ((Number) pos.get(0)).doubleValue(), offsetY + ((Number) pos.get(1)).doubleValue(),
            offsetZ + ((Number) pos.get(2)).doubleValue(),
            yaw != null ? yaw : rotated ? ((Number) rotation.get(0)).floatValue() : 0.0F,
            rotated ? ((Number) rotation.get(1)).floatValue() : 0.0F);
        ((org.basaltmc.bukkit.entity.Cored) made).core().seedNbt(Nbt.writeNamed(nbt));
        return made;
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
        placeTransformed(location.getWorld(), null, location.getBlockX(), location.getBlockY(), location.getBlockZ(),
            includeEntities, rotation, mirror, palette, integrity, random, blockTransformers, entityTransformers);
    }

    private void checkFits(LimitedRegion limited, BlockVector location, StructureRotation rotation, Mirror mirror) {
        BlockVector size = getSize();
        int[][] corners = {{0, 0}, {size.getBlockX() - 1, 0}, {0, size.getBlockZ() - 1},
            {size.getBlockX() - 1, size.getBlockZ() - 1}};
        for (int[] corner : corners) {
            int[] turned = turn(corner[0], corner[1], rotation, mirror);
            if (!limited.isInRegion(location.getBlockX() + turned[0], location.getBlockY(),
                location.getBlockZ() + turned[1])) {
                throw new IllegalArgumentException("The structure does not fit in the region of chunk "
                    + limited.getCenterChunkX() + ", " + limited.getCenterChunkZ());
            }
        }
    }

    private static int[] turn(int x, int z, StructureRotation rotation, Mirror mirror) {
        int flippedX = mirror == Mirror.FRONT_BACK ? -x : x;
        int flippedZ = mirror == Mirror.LEFT_RIGHT ? -z : z;
        return switch (rotation) {
            case CLOCKWISE_90 -> new int[] {-flippedZ, flippedX};
            case CLOCKWISE_180 -> new int[] {-flippedX, -flippedZ};
            case COUNTERCLOCKWISE_90 -> new int[] {flippedZ, -flippedX};
            default -> new int[] {flippedX, flippedZ};
        };
    }

    @Override
    public void place(RegionAccessor regionAccessor, BlockVector location, boolean includeEntities,
            StructureRotation rotation, Mirror mirror, int palette, float integrity, Random random) {
        World world;
        if (regionAccessor instanceof LimitedRegion limited) {
            world = limited.getWorld();
            checkFits(limited, location, rotation, mirror);
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
        LimitedRegion limited = regionAccessor instanceof LimitedRegion region ? region : null;
        World world = limited != null ? limited.getWorld() : regionAccessor instanceof World whole ? whole : null;
        if (world == null) {
            throw new IllegalArgumentException("A structure is placed in a world or a limited region");
        }
        if (limited != null) {
            checkFits(limited, location, rotation, mirror);
        }
        placeTransformed(world, limited, location.getBlockX(), location.getBlockY(), location.getBlockZ(),
            includeEntities, rotation, mirror, palette, integrity, random, blockTransformers, entityTransformers);
    }

    private record Target(int x, int y, int z, int state, Map<String, Object> nbt, BlockState before) {}

    /** Places the structure, then lets the transformers have their say over each block and entity it laid down. */
    private void placeTransformed(World world, LimitedRegion given, int x, int y, int z, boolean includeEntities,
            StructureRotation rotation, Mirror mirror, int palette, float integrity, Random random,
            Collection<BlockTransformer> blockTransformers, Collection<EntityTransformer> entityTransformers) {
        boolean blocksNeeded = blockTransformers != null && !blockTransformers.isEmpty();
        boolean entitiesNeeded = includeEntities && entityTransformers != null && !entityTransformers.isEmpty();
        if (!blocksNeeded && !entitiesNeeded) {
            placeAt(world, x, y, z, includeEntities, rotation, mirror, palette, integrity, random);
            return;
        }
        if (!(world instanceof BasaltWorld basalt)) {
            throw new IllegalArgumentException("A structure is placed in a world the server runs");
        }
        if (palette < -1 || palette >= getPaletteCount()) {
            throw new IllegalArgumentException("Palette index out of range");
        }
        int chosen = palette >= 0 ? palette : getPaletteCount() > 1 ? random.nextInt(getPaletteCount()) : 0;
        List<Object> entries = palettes().get(chosen);
        LimitedRegion region = given;
        if (region == null) {
            BlockVector size = getSize();
            region = org.basaltmc.bukkit.worldgen.BasaltLimitedRegion.of(basalt, x >> 4, z >> 4,
                Math.max(size.getBlockX(), size.getBlockZ()) + 16);
        }
        List<Target> targets = new ArrayList<>();
        List<Object> blocks = Nbt.list(data.get("blocks"));
        for (Object value : blocks == null ? List.<Object>of() : blocks) {
            Map<String, Object> block = Nbt.map(value);
            List<Object> pos = block == null ? null : Nbt.list(block.get("pos"));
            int state = block == null ? -1 : Nbt.integer(block, "state", -1);
            if (pos == null || pos.size() < 3 || state < 0 || state >= entries.size()) {
                continue;
            }
            Map<String, Object> entry = Nbt.map(entries.get(state));
            if (entry != null && "minecraft:structure_void".equals(Nbt.string(entry, "Name", ""))) {
                continue;
            }
            int[] turned = turn(((Number) pos.get(0)).intValue(), ((Number) pos.get(2)).intValue(), rotation, mirror);
            int at = y + ((Number) pos.get(1)).intValue();
            targets.add(new Target(x + turned[0], at, z + turned[1], state, Nbt.map(block.get("nbt")),
                world.getBlockAt(x + turned[0], at, z + turned[1]).getState()));
        }
        placeAt(world, x, y, z, entitiesNeeded ? false : includeEntities, rotation, mirror, chosen, integrity, random);
        for (Target target : targets) {
            BlockData placed = blockData(entries.get(target.state));
            BlockData turned = placed.clone();
            turned.mirror(mirror);
            turned.rotate(rotation);
            BlockState original = new BasaltBlockState(basalt, target.x, target.y, target.z, placed);
            BlockState current = new BasaltBlockState(basalt, target.x, target.y, target.z, turned);
            if (target.nbt != null) {
                current = org.basaltmc.bukkit.tile.TileStates.detached(current, Nbt.deepCopy(target.nbt),
                    Nbt.list(target.nbt, "Items"));
            }
            BlockTransformer.TransformationState seen = new BlockTransformer.TransformationState() {
                @Override
                public BlockState getOriginal() {
                    return original;
                }

                @Override
                public BlockState getWorld() {
                    return target.before;
                }
            };
            BlockState result = current;
            for (BlockTransformer transformer : blockTransformers == null ? List.<BlockTransformer>of()
                : blockTransformers) {
                result = transformer.transform(region, target.x, target.y, target.z, result, seen);
            }
            if (result != current || org.basaltmc.bukkit.tile.TileStates.dataOf(result) != null
                || !result.getBlockData().equals(basalt.getBlockAt(target.x, target.y, target.z).getBlockData())) {
                result.copy(new Location(basalt, target.x, target.y, target.z)).update(true, false);
            }
        }
        if (entitiesNeeded) {
            List<Object> saved = Nbt.list(data.get("entities"));
            for (Object value : saved == null ? List.<Object>of() : saved) {
                Map<String, Object> entry = Nbt.map(value);
                List<Object> pos = entry == null ? null : Nbt.list(entry.get("pos"));
                Map<String, Object> nbt = entry == null ? null : Nbt.map(entry.get("nbt"));
                if (pos == null || pos.size() < 3 || nbt == null) {
                    continue;
                }
                double[] moved = turnPoint(((Number) pos.get(0)).doubleValue(), ((Number) pos.get(2)).doubleValue(),
                    rotation, mirror);
                List<Object> facing = Nbt.list(nbt.get("Rotation"));
                float yaw = facing == null || facing.isEmpty() ? 0.0F : ((Number) facing.get(0)).floatValue();
                Entity entity = entityOf(basalt, entry, x + moved[0] - ((Number) pos.get(0)).doubleValue(),
                    y, z + moved[1] - ((Number) pos.get(2)).doubleValue(), turnYaw(yaw, rotation, mirror));
                if (entity == null) {
                    continue;
                }
                Location at = entity.getLocation();
                boolean allowed = true;
                for (EntityTransformer transformer : entityTransformers) {
                    allowed = transformer.transform(region, at.getBlockX(), at.getBlockY(), at.getBlockZ(), entity,
                        allowed);
                }
                if (allowed) {
                    basalt.addEntity(entity);
                }
            }
        }
    }

    /** A template point as it lands once mirrored and turned about the origin of its block. */
    private static double[] turnPoint(double x, double z, StructureRotation rotation, Mirror mirror) {
        double flippedX = mirror == Mirror.FRONT_BACK ? 1.0 - x : x;
        double flippedZ = mirror == Mirror.LEFT_RIGHT ? 1.0 - z : z;
        return switch (rotation) {
            case COUNTERCLOCKWISE_90 -> new double[] {flippedZ, 1.0 - flippedX};
            case CLOCKWISE_90 -> new double[] {1.0 - flippedZ, flippedX};
            case CLOCKWISE_180 -> new double[] {1.0 - flippedX, 1.0 - flippedZ};
            default -> new double[] {flippedX, flippedZ};
        };
    }

    private static float turnYaw(float yaw, StructureRotation rotation, Mirror mirror) {
        float wrapped = yaw;
        float mirrored = switch (mirror) {
            case LEFT_RIGHT -> 180.0F - wrapped;
            case FRONT_BACK -> -wrapped;
            default -> wrapped;
        };
        return switch (rotation) {
            case CLOCKWISE_90 -> mirrored + 90.0F;
            case CLOCKWISE_180 -> mirrored + 180.0F;
            case COUNTERCLOCKWISE_90 -> mirrored + 270.0F;
            default -> mirrored;
        };
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
