// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.core.TypedInstance;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.debug.DebugValueSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.basaltmc.bukkit.facade.ValueIo;
import net.minecraft.CrashReportCategory;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentMap;
import net.minecraft.core.component.DataComponentPatch;
import net.minecraft.network.chat.Component;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.util.ProblemReporter;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.ChunkPos;

public class BlockEntity implements DebugValueSource, TypedInstance<BlockEntityType<?>> {
    protected boolean remove;

    private ServerLevel level;
    private BlockPos pos;
    private BlockEntityType<?> type;
    private CompoundTag snapshot;

    protected BlockEntity() {
    }

    public BlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        this.level = level;
        this.pos = pos.immutable();
        this.worldPosition = this.pos;
        this.snapshot = nbt;
        String id = nbt.getStringOr("id", "minecraft:air");
        this.type = BlockEntityType.of(Identifier.parse(id));
    }

    private String world() {
        return level.getWorld().getName();
    }

    protected <T extends org.bukkit.block.BlockState> T bukkit(Class<T> type) {
        if (getLevel() instanceof ServerLevel level) {
            BlockPos at = getBlockPos();
            org.bukkit.block.BlockState state = level.getWorld().getBlockAt(at.getX(), at.getY(), at.getZ()).getState();
            return type.isInstance(state) ? type.cast(state) : null;
        }
        return null;
    }

    public BlockPos getBlockPos() {
        return pos;
    }

    public BlockState getBlockState() {
        return level.getBlockState(pos);
    }

    public Level getLevel() {
        return level;
    }

    public BlockEntityType getType() {
        return type;
    }

    public boolean isRemoved() {
        return remove || (level != null && WorldNatives.blockEntityNbt(world(), pos.getX(), pos.getY(), pos.getZ()) == null);
    }

    public CompoundTag nbt() {
        if (level == null) {
            return snapshot.copy();
        }
        CompoundTag live = NbtCodec.fromBytes(WorldNatives.blockEntityNbt(world(), pos.getX(), pos.getY(), pos.getZ()));
        return live != null ? live : snapshot.copy();
    }

    public void saveWithId(ValueOutput output) {
        ValueIo.write(output, nbt());
    }

    public void loadWithComponents(ValueInput input) {
        load(ValueIo.read(input));
    }

    public void load(CompoundTag tag) {
        if (level == null) {
            snapshot = tag.copy();
            return;
        }
        byte[] bytes = NbtCodec.toBytes(tag);
        if (!WorldNatives.setBlockEntityNbt(world(), pos.getX(), pos.getY(), pos.getZ(), bytes)
            && !WorldNatives.createBlockEntity(world(), pos.getX(), pos.getY(), pos.getZ(), bytes)) {
            return;
        }
        remove = false;
    }

    public void setBlockState(BlockState blockState) {
    }

    public void setChanged() {
    }
    protected BlockPos worldPosition;
    public BlockEntity(BlockEntityType<?> type, BlockPos worldPosition, BlockState blockState) {
        this.type = type;
        this.pos = worldPosition.immutable();
        this.worldPosition = this.pos;
        this.snapshot = new CompoundTag();
    }
    public boolean isValidBlockState(BlockState blockState) {
        return blockState.hasBlockEntity();
    }
    public static BlockPos getPosFromTag(ChunkPos base, CompoundTag entityTag) {
        return new BlockPos(entityTag.getIntOr("x", 0), entityTag.getIntOr("y", 0), entityTag.getIntOr("z", 0));
    }
    public void setLevel(Level level) {
        if (level instanceof ServerLevel server) {
            this.level = server;
        }
    }
    public boolean hasLevel() {
        return level != null;
    }
    protected void loadAdditional(ValueInput input) {
        load(org.basaltmc.bukkit.facade.ValueIo.read(input));
    }
    public void loadCustomOnly(ValueInput input) {
        load(org.basaltmc.bukkit.facade.ValueIo.read(input));
    }
    protected void saveAdditional(ValueOutput output) {
        org.basaltmc.bukkit.facade.ValueIo.write(output, saveWithoutMetadata((HolderLookup.Provider) null));
    }
    public CompoundTag saveWithFullMetadata(HolderLookup.Provider registries) {
        net.minecraft.nbt.CompoundTag tag = nbt();
        tag.putString("id", type == null ? "minecraft:air" : type.key().toString());
        tag.putInt("x", pos.getX());
        tag.putInt("y", pos.getY());
        tag.putInt("z", pos.getZ());
        return tag;
    }
    public void saveWithFullMetadata(ValueOutput output) {
        org.basaltmc.bukkit.facade.ValueIo.write(output, saveWithFullMetadata((HolderLookup.Provider) null));
    }
    public CompoundTag saveWithoutMetadata(HolderLookup.Provider registries) {
        net.minecraft.nbt.CompoundTag tag = nbt();
        tag.remove("id");
        tag.remove("x");
        tag.remove("y");
        tag.remove("z");
        return tag;
    }
    public void saveWithoutMetadata(ValueOutput output) {
        org.basaltmc.bukkit.facade.ValueIo.write(output, saveWithoutMetadata((HolderLookup.Provider) null));
    }
    public CompoundTag saveCustomOnly(HolderLookup.Provider registries) {
        net.minecraft.nbt.CompoundTag tag = saveWithoutMetadata(registries);
        tag.remove("components");
        return tag;
    }
    public void saveCustomOnly(ValueOutput output) {
        org.basaltmc.bukkit.facade.ValueIo.write(output, saveCustomOnly((HolderLookup.Provider) null));
    }
    public static void addEntityType(ValueOutput output, BlockEntityType<?> type) {
        output.putString("id", type.key().toString());
    }
    public static BlockEntity loadStatic(BlockPos pos, BlockState state, CompoundTag tag, HolderLookup.Provider registries) {
        String id = tag.getStringOr("id", "");
        if (id.isEmpty()) {
            return null;
        }
        return org.basaltmc.bukkit.facade.BlockEntityTypes.view(null, pos, tag);
    }
    protected static void setChanged(Level level, BlockPos worldPosition, BlockState blockState) {

    }
    public Packet<ClientGamePacketListener> getUpdatePacket() {
        return net.minecraft.network.protocol.game.ClientboundBlockEntityDataPacket.create(this);
    }
    public CompoundTag getUpdateTag(HolderLookup.Provider registries) {
        return saveWithoutMetadata(registries);
    }
    public void setRemoved() {
        remove = true;
    }
    public void clearRemoved() {
        remove = false;
    }
    public void preRemoveSideEffects(BlockPos pos, BlockState state) {

    }
    public boolean triggerEvent(int b0, int b1) {
        return false;
    }
    public void fillCrashReportCategory(CrashReportCategory category) {

    }
    public String getNameForReporting() {
        return String.valueOf(type);
    }
    public Holder<BlockEntityType<?>> typeHolder() {
        return (net.minecraft.core.Holder.Reference<BlockEntityType<?>>) type.builtInRegistryHolder();
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BlockEntity#applyImplicitComponents");
    }
    public void applyComponentsFromItemStack(ItemStack stack) {

    }
    public void applyComponents(DataComponentMap prototype, DataComponentPatch patch) {

    }
    protected void collectImplicitComponents(DataComponentMap.Builder components) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BlockEntity#collectImplicitComponents");
    }
    public void removeComponentsFromTag(ValueOutput output) {

    }
    public DataComponentMap collectComponents() {
        return net.minecraft.core.component.DataComponentMap.EMPTY;
    }
    public DataComponentMap components() {
        return net.minecraft.core.component.DataComponentMap.EMPTY;
    }
    public void setComponents(DataComponentMap components) {

    }
    public static Component parseCustomNameSafe(ValueInput input, String name) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BlockEntity#parseCustomNameSafe");
    }
    public ProblemReporter.PathElement problemPath() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BlockEntity#problemPath");
    }
    public void registerDebugValues(ServerLevel level, DebugValueSource.Registration registration) {

    }
    static record BlockEntityPathElement(BlockEntity blockEntity) implements ProblemReporter.PathElement {
        public String get() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BlockEntity.BlockEntityPathElement#get");
        }
    }
}
