// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import org.basaltmc.bukkit.facade.TileNbt;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import java.util.List;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.Vec3i;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.protocol.game.ClientboundBlockEntityDataPacket;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.state.properties.StructureMode;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class StructureBlockEntity extends BlockEntity implements BoundingBoxRenderable {
    public StructureBlockEntity(BlockPos blockPos, BlockState blockState) {
        super(BlockEntityType.named("structure_block"), blockPos, blockState);
    }

    public StructureBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    private static <E extends Enum<E>> E enumOf(Class<E> type, String name, E fallback) {
        for (E value : type.getEnumConstants()) {
            if (value.name().equalsIgnoreCase(name)) {
                return value;
            }
        }
        return fallback;
    }

    private static int clamp(int value, int low, int high) {
        return Math.max(low, Math.min(high, value));
    }
    public static final int MAX_OFFSET_PER_AXIS = 48;
    public static final int MAX_SIZE_PER_AXIS = 48;
    public static final String AUTHOR_TAG = "author";
    protected void saveAdditional(ValueOutput output) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.StructureBlockEntity#saveAdditional");
    }
    protected void loadAdditional(ValueInput input) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.StructureBlockEntity#loadAdditional");
    }
    public boolean usedBy(Player player) {
        return false;
    }
    public String getStructureName() {
        return nbt().getStringOr("name", "");
    }
    public boolean hasStructureName() {
        return !getStructureName().isEmpty();
    }
    public void setStructureName(String structureName) {
        TileNbt.update(this, tag -> tag.putString("name", structureName == null ? "" : structureName));
    }
    public void setStructureName(Identifier structureName) {
        setStructureName(structureName == null ? null : structureName.toString());
    }
    public void createdBy(LivingEntity creator) {
        TileNbt.update(this, tag -> tag.putString("author", creator.getName().getString()));
    }
    public BlockPos getStructurePos() {
        CompoundTag tag = nbt();
        return new BlockPos(clamp(tag.getIntOr("posX", 0), -48, 48), clamp(tag.getIntOr("posY", 1), -48, 48),
            clamp(tag.getIntOr("posZ", 0), -48, 48));
    }
    public void setStructurePos(BlockPos structurePos) {
        TileNbt.update(this, tag -> {
            tag.putInt("posX", clamp(structurePos.getX(), -48, 48));
            tag.putInt("posY", clamp(structurePos.getY(), -48, 48));
            tag.putInt("posZ", clamp(structurePos.getZ(), -48, 48));
        });
    }
    public Vec3i getStructureSize() {
        CompoundTag tag = nbt();
        return new Vec3i(clamp(tag.getIntOr("sizeX", 0), 0, 48), clamp(tag.getIntOr("sizeY", 0), 0, 48),
            clamp(tag.getIntOr("sizeZ", 0), 0, 48));
    }
    public void setStructureSize(Vec3i structureSize) {
        TileNbt.update(this, tag -> {
            tag.putInt("sizeX", clamp(structureSize.getX(), 0, 48));
            tag.putInt("sizeY", clamp(structureSize.getY(), 0, 48));
            tag.putInt("sizeZ", clamp(structureSize.getZ(), 0, 48));
        });
    }
    public Mirror getMirror() {
        return enumOf(Mirror.class, nbt().getStringOr("mirror", "NONE"), Mirror.NONE);
    }
    public void setMirror(Mirror mirror) {
        TileNbt.update(this, tag -> tag.putString("mirror", mirror.name()));
    }
    public Rotation getRotation() {
        return enumOf(Rotation.class, nbt().getStringOr("rotation", "NONE"), Rotation.NONE);
    }
    public void setRotation(Rotation rotation) {
        TileNbt.update(this, tag -> tag.putString("rotation", rotation.name()));
    }
    public String getMetaData() {
        return nbt().getStringOr("metadata", "");
    }
    public void setMetaData(String metaData) {
        TileNbt.update(this, tag -> tag.putString("metadata", metaData));
    }
    public StructureMode getMode() {
        return enumOf(StructureMode.class, nbt().getStringOr("mode", "DATA"), StructureMode.DATA);
    }
    public void setMode(StructureMode mode) {
        TileNbt.update(this, tag -> tag.putString("mode", mode.name()));
    }
    public boolean isIgnoreEntities() {
        return nbt().getBooleanOr("ignoreEntities", true);
    }
    public boolean isStrict() {
        return nbt().getBooleanOr("strict", false);
    }
    public void setIgnoreEntities(boolean ignoreEntities) {
        TileNbt.update(this, tag -> tag.putBoolean("ignoreEntities", ignoreEntities));
    }
    public void setStrict(boolean strict) {
        TileNbt.update(this, tag -> tag.putBoolean("strict", strict));
    }
    public float getIntegrity() {
        return nbt().getFloatOr("integrity", 1.0F);
    }
    public void setIntegrity(float integrity) {
        TileNbt.update(this, tag -> tag.putFloat("integrity", Math.max(0.0F, Math.min(1.0F, integrity))));
    }
    public long getSeed() {
        return nbt().getLongOr("seed", 0L);
    }
    public void setSeed(long seed) {
        TileNbt.update(this, tag -> tag.putLong("seed", seed));
    }
    public boolean detectSize() {
        if (getMode() != StructureMode.SAVE || !(getLevel() instanceof ServerLevel server) || !hasStructureName()) {
            return false;
        }
        String world = server.getWorld().getName();
        BlockPos here = getBlockPos();
        int minX = here.getX(), minY = here.getY(), minZ = here.getZ();
        int maxX = minX, maxY = minY, maxZ = minZ;
        int corners = 0;
        for (int cx = (here.getX() - 80) >> 4; cx <= (here.getX() + 80) >> 4; cx++) {
            for (int cz = (here.getZ() - 80) >> 4; cz <= (here.getZ() + 80) >> 4; cz++) {
                int[] flat = org.basaltmc.bukkit.WorldNatives.blockEntityPositions(world, cx, cz);
                for (int n = 0; flat != null && n + 2 < flat.length; n += 3) {
                    byte[] raw = org.basaltmc.bukkit.WorldNatives.blockEntityNbt(world, flat[n], flat[n + 1], flat[n + 2]);
                    CompoundTag tag = raw == null ? null : org.basaltmc.bukkit.facade.NbtCodec.fromBytes(raw);
                    if (tag != null && tag.getStringOr("id", "").equals("minecraft:structure_block")
                        && tag.getStringOr("mode", "").equalsIgnoreCase("CORNER")
                        && tag.getStringOr("name", "").equals(getStructureName())) {
                        minX = Math.min(minX, flat[n]);
                        minY = Math.min(minY, flat[n + 1]);
                        minZ = Math.min(minZ, flat[n + 2]);
                        maxX = Math.max(maxX, flat[n]);
                        maxY = Math.max(maxY, flat[n + 1]);
                        maxZ = Math.max(maxZ, flat[n + 2]);
                        corners++;
                    }
                }
            }
        }
        Vec3i size = new Vec3i(maxX - minX - 1, maxY - minY - 1, maxZ - minZ - 1);
        if (corners < 1 || size.getX() < 1 || size.getY() < 1 || size.getZ() < 1) {
            return false;
        }
        setStructurePos(new BlockPos(minX - here.getX() + 1, minY - here.getY() + 1, minZ - here.getZ() + 1));
        setStructureSize(size);
        return true;
    }
    public boolean saveStructure() {
        return saveStructure(true);
    }
    public boolean saveStructure(boolean saveToDisk) {
        if (getMode() != StructureMode.SAVE || !hasStructureName() || !(getLevel() instanceof ServerLevel server)) {
            return false;
        }
        BlockPos origin = getBlockPos().offset(getStructurePos());
        return saveStructure(server, Identifier.parse(getStructureName()), origin, getStructureSize(), isIgnoreEntities(),
            nbt().getStringOr("author", ""), saveToDisk, List.of(net.minecraft.world.level.block.Blocks.STRUCTURE_VOID));
    }
    public static boolean saveStructure(ServerLevel level, Identifier structureName, BlockPos pos, Vec3i structureSize, boolean ignoreEntities, String author, boolean saveToDisk, List<Block> ignoreBlocks) {
        if (!saveToDisk) {
            return true;
        }
        return org.basaltmc.bukkit.facade.StructureFiles.save(level, structureName, pos, structureSize, author, ignoreBlocks);
    }
    public static RandomSource createRandom(long seed) {
        return seed == 0L ? RandomSource.create(System.currentTimeMillis()) : RandomSource.create(seed);
    }
    public boolean placeStructureIfSameSize(ServerLevel level) {
        if (!hasStructureName()) {
            return false;
        }
        CompoundTag root = org.basaltmc.bukkit.facade.StructureFiles.read(level, Identifier.parse(getStructureName()));
        if (root == null || !org.basaltmc.bukkit.facade.StructureFiles.sizeOf(root).equals(getStructureSize())) {
            return false;
        }
        placeStructure(level);
        return true;
    }
    public boolean loadStructureInfo(ServerLevel level) {
        if (!hasStructureName()) {
            return false;
        }
        CompoundTag root = org.basaltmc.bukkit.facade.StructureFiles.read(level, Identifier.parse(getStructureName()));
        if (root == null) {
            return false;
        }
        setStructureSize(org.basaltmc.bukkit.facade.StructureFiles.sizeOf(root));
        return true;
    }
    public void placeStructure(ServerLevel level) {
        CompoundTag root = hasStructureName()
            ? org.basaltmc.bukkit.facade.StructureFiles.read(level, Identifier.parse(getStructureName())) : null;
        if (root != null) {
            org.basaltmc.bukkit.facade.StructureFiles.place(level, root, getBlockPos().offset(getStructurePos()),
                getMirror(), getRotation(), getIntegrity(), getSeed());
        }
    }
    public void unloadStructure() {
        
    }
    public boolean isStructureLoadable() {
        return getLevel() instanceof ServerLevel server && hasStructureName()
            && org.basaltmc.bukkit.facade.StructureFiles.exists(server, Identifier.parse(getStructureName()));
    }
    public boolean isPowered() {
        return nbt().getBooleanOr("powered", false);
    }
    public void setPowered(boolean powered) {
        TileNbt.update(this, tag -> tag.putBoolean("powered", powered));
    }
    public boolean getShowAir() {
        return nbt().getBooleanOr("showair", false);
    }
    public void setShowAir(boolean showAir) {
        TileNbt.update(this, tag -> tag.putBoolean("showair", showAir));
    }
    public boolean getShowBoundingBox() {
        return nbt().getBooleanOr("showboundingbox", true);
    }
    public void setShowBoundingBox(boolean showBoundingBox) {
        TileNbt.update(this, tag -> tag.putBoolean("showboundingbox", showBoundingBox));
    }
    public BoundingBoxRenderable.Mode renderMode() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.StructureBlockEntity#renderMode");
    }
    public BoundingBoxRenderable.RenderableBox getRenderableBox() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.StructureBlockEntity#getRenderableBox");
    }
    public static enum UpdateType {
        UPDATE_DATA, SAVE_AREA, LOAD_AREA, SCAN_AREA;
    }
}
