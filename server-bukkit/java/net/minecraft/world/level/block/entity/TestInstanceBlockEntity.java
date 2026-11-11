// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;
import java.util.ArrayList;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.core.registries.Registries;
import net.minecraft.util.ARGB;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.entity.BeaconBeamOwner;
import org.basaltmc.bukkit.facade.ValueIo;
import net.minecraft.ChatFormatting;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.Vec3i;
import net.minecraft.gametest.framework.GameTestInstance;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundBlockEntityDataPacket;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.AABB;
public class TestInstanceBlockEntity extends BlockEntity implements BoundingBoxRenderable, BeaconBeamOwner {
    private static final Component kInvalidName = Component.translatable("test_instance_block.invalid_test");
    private static final Vec3i kStructureOffset = new Vec3i(0, 1, 1);
    private static final List<BeaconBeamOwner.Section> kBeamCleared = List.of();
    private static final List<BeaconBeamOwner.Section> kBeamRunning = List.of(new BeaconBeamOwner.Section(ARGB.color(128, 128, 128)));
    private static final List<BeaconBeamOwner.Section> kBeamSuccess = List.of(new BeaconBeamOwner.Section(ARGB.color(0, 255, 0)));
    private static final List<BeaconBeamOwner.Section> kBeamFailed = List.of(new BeaconBeamOwner.Section(ARGB.color(255, 0, 0)));
    public TestInstanceBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }
    private Data data() {
        CompoundTag stored = nbt().getCompoundOrEmpty("data");
        Optional<ResourceKey<GameTestInstance>> test = stored.getString("test").map(id -> ResourceKey.create(Registries.TEST_INSTANCE, Identifier.parse(id)));
        int[] size = stored.getIntArray("size").filter(a -> a.length == 3).orElse(new int[3]);
        String spin = stored.getStringOr("rotation", "none");
        Rotation rotation = Rotation.NONE;
        for (Rotation candidate : Rotation.values()) {
            if (candidate.getSerializedName().equals(spin)) {
                rotation = candidate;
            }
        }
        Status status = Status.valueOf(stored.getStringOr("status", "cleared").toUpperCase(java.util.Locale.ROOT));
        Optional<Component> error = Optional.ofNullable(stored.get("error_message"))
            .flatMap(raw -> ComponentSerialization.CODEC.parse(NbtOps.INSTANCE, raw).result());
        return new Data(test, new Vec3i(size[0], size[1], size[2]), rotation, stored.getBooleanOr("ignore_entities", false), status, error);
    }
    private void storeData(Data now) {
        CompoundTag stored = new CompoundTag();
        now.test().ifPresent(key -> stored.putString("test", key.identifier().toString()));
        stored.putIntArray("size", new int[] {now.size().getX(), now.size().getY(), now.size().getZ()});
        stored.putString("rotation", now.rotation().getSerializedName());
        stored.putBoolean("ignore_entities", now.ignoreEntities());
        stored.putString("status", now.status().getSerializedName());
        now.errorMessage().flatMap(text -> ComponentSerialization.CODEC.encodeStart(NbtOps.INSTANCE, text).result()).ifPresent(tag -> stored.put("error_message", tag));
        CompoundTag whole = nbt();
        whole.put("data", stored);
        load(whole);
    }
    private int padding() {
        return 0;
    }
    private Vec3i transformedSize() {
        Vec3i size = getSize();
        boolean swapped = getRotation() == Rotation.CLOCKWISE_90 || getRotation() == Rotation.COUNTERCLOCKWISE_90;
        return new Vec3i(swapped ? size.getZ() : size.getX(), size.getY(), swapped ? size.getX() : size.getZ());
    }
    public TestInstanceBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("test_instance_block"), worldPosition, blockState);
    }
    public void set(Data data) {
        storeData(data);
    }
    public static Optional<Vec3i> getStructureSize(ServerLevel level, ResourceKey<GameTestInstance> testKey) {
        return Optional.empty();
    }
    public BoundingBox getStructureBoundingBox() {
        BlockPos first = getStructurePos();
        return BoundingBox.fromCorners(first, first.offset(transformedSize()).offset(-1, -1, -1));
    }
    public BoundingBox getTestBoundingBox() {
        return getStructureBoundingBox().inflatedBy(padding());
    }
    public AABB getStructureBounds() {
        return AABB.of(getStructureBoundingBox());
    }
    public AABB getTestBounds() {
        return getStructureBounds().inflate(padding());
    }
    public Optional<ResourceKey<GameTestInstance>> test() {
        return data().test();
    }
    public Component getTestName() {
        return test().map(key -> (Component) Component.literal(key.identifier().toString())).orElse(kInvalidName);
    }
    public boolean ignoreEntities() {
        return data().ignoreEntities();
    }
    public Vec3i getSize() {
        return data().size();
    }
    public Rotation getRotation() {
        return data().rotation();
    }
    public Optional<Component> errorMessage() {
        return data().errorMessage();
    }
    public void setErrorMessage(Component errorMessage) {
        set(data().withError(errorMessage));
    }
    public void setSuccess() {
        set(data().withStatus(Status.FINISHED));
    }
    public void setRunning() {
        set(data().withStatus(Status.RUNNING));
    }
    public void setChanged() {
        super.setChanged();
    }
    public ClientboundBlockEntityDataPacket getUpdatePacket() {
        return ClientboundBlockEntityDataPacket.create(this);
    }
    public CompoundTag getUpdateTag(HolderLookup.Provider registries) {
        return saveCustomOnly(registries);
    }
    protected void loadAdditional(ValueInput input) {
        load(ValueIo.read(input));
    }
    protected void saveAdditional(ValueOutput output) {
        ValueIo.write(output, saveWithoutMetadata((HolderLookup.Provider) null));
    }
    public BoundingBoxRenderable.Mode renderMode() {
        return BoundingBoxRenderable.Mode.BOX;
    }
    public BlockPos getStructurePos() {
        int pad = padding();
        return getStructurePos(getBlockPos().offset(pad, pad, pad));
    }
    public static BlockPos getStructurePos(BlockPos blockPos) {
        return blockPos.offset(kStructureOffset);
    }
    public BoundingBoxRenderable.RenderableBox getRenderableBox() {
        int pad = padding();
        return new BoundingBoxRenderable.RenderableBox(new BlockPos(kStructureOffset.getX() + pad, kStructureOffset.getY() + pad, kStructureOffset.getZ() + pad), transformedSize());
    }
    public List<BeaconBeamOwner.Section> getBeamSections() {
        Data now = data();
        return switch (now.status()) {
            case CLEARED -> kBeamCleared;
            case RUNNING -> kBeamRunning;
            case FINISHED -> now.errorMessage().isEmpty() ? kBeamSuccess : kBeamFailed;
        };
    }
    public void resetTest(Consumer<Component> feedbackOutput) {
        removeBarriers();
        clearErrorMarkers();
        if (placeStructure()) {
            feedbackOutput.accept(Component.translatable("test_instance_block.reset_success", getTestName()).withStyle(ChatFormatting.GREEN));
        }
        set(data().withStatus(Status.CLEARED));
    }
    public Optional<Identifier> saveTest(Consumer<Component> feedbackOutput) {
        BlockPos at = getBlockPos();
        feedbackOutput.accept(Component.translatable("test_instance_block.error.unable_to_save", at.getX(), at.getY(), at.getZ()).withStyle(ChatFormatting.RED));
        return Optional.empty();
    }
    public boolean exportTest(Consumer<Component> feedbackOutput) {
        return saveTest(feedbackOutput).isPresent() && getLevel() instanceof ServerLevel;
    }
    public static boolean export(ServerLevel level, Identifier structureId, Consumer<Component> feedbackOutput) {
        feedbackOutput.accept(Component.literal("Test structure exporting is disabled").withStyle(ChatFormatting.RED));
        return true;
    }
    public void runTest(Consumer<Component> feedbackOutput) {
        BlockPos at = getBlockPos();
        feedbackOutput.accept(Component.translatable("test_instance_block.error.no_test", at.getX(), at.getY(), at.getZ()).withStyle(ChatFormatting.RED));
    }
    public boolean placeStructure() {
        return false;


    }
    public BlockPos getStartCorner() {
        Vec3i size = getSize();
        BlockPos corner = getStructurePos();
        return switch (getRotation()) {
            case NONE -> corner;
            case CLOCKWISE_90 -> corner.offset(size.getZ() - 1, 0, 0);
            case CLOCKWISE_180 -> corner.offset(size.getX() - 1, 0, size.getZ() - 1);
            case COUNTERCLOCKWISE_90 -> corner.offset(0, 0, size.getX() - 1);
        };
    }
    public void encaseStructure() {
        processStructureBoundary(pos -> {
            if (!getLevel().getBlockState(pos).is(Blocks.TEST_INSTANCE_BLOCK)) {
                getLevel().setBlockAndUpdate(pos, Blocks.BARRIER.defaultBlockState());
            }
        });
    }
    public void removeBarriers() {
        processStructureBoundary(pos -> {
            if (getLevel().getBlockState(pos).is(Blocks.BARRIER)) {
                getLevel().setBlockAndUpdate(pos, Blocks.AIR.defaultBlockState());
            }
        });
    }
    public void processStructureBoundary(Consumer<BlockPos> action) {
        AABB bounds = getStructureBounds();
        BlockPos low = BlockPos.containing(bounds.minX, bounds.minY, bounds.minZ).offset(-1, -1, -1);
        BlockPos high = BlockPos.containing(bounds.maxX, bounds.maxY, bounds.maxZ);
        BlockPos.betweenClosedStream(low, high).forEach(pos -> {
            boolean wall = pos.getX() == low.getX() || pos.getX() == high.getX() || pos.getZ() == low.getZ()
                || pos.getZ() == high.getZ() || pos.getY() == low.getY();
            if (wall || pos.getY() == high.getY()) {
                action.accept(pos);
            }
        });
    }
    public void markError(BlockPos pos, Component text) {
        CompoundTag whole = nbt();
        ListTag marks = whole.getListOrEmpty("errors");
        CompoundTag mark = new CompoundTag();
        mark.putIntArray("pos", new int[] {pos.getX(), pos.getY(), pos.getZ()});
        ComponentSerialization.CODEC.encodeStart(NbtOps.INSTANCE, text).result().ifPresent(tag -> mark.put("text", tag));
        marks.add(mark);
        whole.put("errors", marks);
        load(whole);
    }
    public void clearErrorMarkers() {
        CompoundTag whole = nbt();
        if (whole.contains("errors")) {
            whole.remove("errors");
            load(whole);
        }
    }
    public List<ErrorMarker> getErrorMarkers() {
        List<ErrorMarker> found = new ArrayList<>();
        for (Tag entry : nbt().getListOrEmpty("errors")) {
            if (entry instanceof CompoundTag mark) {
                int[] at = mark.getIntArray("pos").filter(a -> a.length == 3).orElse(new int[3]);
                Component text = mark.get("text") == null ? Component.empty() : ComponentSerialization.CODEC.parse(NbtOps.INSTANCE, mark.get("text")).result().orElse(Component.empty());
                found.add(new ErrorMarker(new BlockPos(at[0], at[1], at[2]), text));
            }
        }
        return found;
    }
    public static enum Status implements StringRepresentable {
        CLEARED, RUNNING, FINISHED;
        public static final Codec<Status> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.TestInstanceBlockEntity.Status.class, "CODEC");
        public static final StreamCodec<ByteBuf, Status> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.block.entity.TestInstanceBlockEntity.Status.class, "STREAM_CODEC");
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
        public static Status byIndex(int index) {
            Status[] all = values();
            return index >= 0 && index < all.length ? all[index] : all[0];
        }
    }
    public static record Data(Optional<ResourceKey<GameTestInstance>> test, Vec3i size, Rotation rotation, boolean ignoreEntities, Status status, Optional<Component> errorMessage) {
        public static final Codec<Data> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.TestInstanceBlockEntity.Data.class, "CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, Data> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.block.entity.TestInstanceBlockEntity.Data.class, "STREAM_CODEC");
        public Data withSize(Vec3i size) {
            return new Data(test, size, rotation, ignoreEntities, status, errorMessage);
        }
        public Data withStatus(Status status) {
            return new Data(test, size, rotation, ignoreEntities, status, errorMessage);
        }
        public Data withError(Component error) {
            return new Data(test, size, rotation, ignoreEntities, Status.FINISHED, Optional.of(error));
        }
    }
    public static record ErrorMarker(BlockPos pos, Component text) {
        public static final Codec<ErrorMarker> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.TestInstanceBlockEntity.ErrorMarker.class, "CODEC");
        public static final Codec<List<ErrorMarker>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.TestInstanceBlockEntity.ErrorMarker.class, "LIST_CODEC");
    }
}
