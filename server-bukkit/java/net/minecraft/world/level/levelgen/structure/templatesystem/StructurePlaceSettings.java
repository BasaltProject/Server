// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
public class StructurePlaceSettings {
    private Mirror mirror = Mirror.NONE;
    private Rotation rotation = Rotation.NONE;
    private BlockPos rotationPivot = BlockPos.ZERO;
    private boolean ignoreEntities;
    private BoundingBox boundingBox;
    private RandomSource random;
    private LiquidSettings liquidSettings = LiquidSettings.APPLY_WATERLOGGING;
    private boolean knownShape;
    private boolean finalizeEntities;
    private final List<StructureProcessor> processors = new java.util.ArrayList<>();
    public StructurePlaceSettings() {

    }
    public StructurePlaceSettings copy() {
        StructurePlaceSettings copy = new StructurePlaceSettings();
        copy.mirror = mirror;
        copy.rotation = rotation;
        copy.rotationPivot = rotationPivot;
        copy.ignoreEntities = ignoreEntities;
        copy.boundingBox = boundingBox;
        copy.random = random;
        copy.liquidSettings = liquidSettings;
        copy.knownShape = knownShape;
        copy.finalizeEntities = finalizeEntities;
        copy.processors.addAll(processors);
        return copy;
    }
    public StructurePlaceSettings setMirror(Mirror mirror) {
        this.mirror = mirror;
        return this;
    }
    public StructurePlaceSettings setRotation(Rotation rotation) {
        this.rotation = rotation;
        return this;
    }
    public StructurePlaceSettings setRotationPivot(BlockPos rotationPivot) {
        this.rotationPivot = rotationPivot;
        return this;
    }
    public StructurePlaceSettings setIgnoreEntities(boolean ignoreEntities) {
        this.ignoreEntities = ignoreEntities;
        return this;
    }
    public StructurePlaceSettings setBoundingBox(BoundingBox boundingBox) {
        this.boundingBox = boundingBox;
        return this;
    }
    public StructurePlaceSettings setRandom(RandomSource random) {
        this.random = random;
        return this;
    }
    public StructurePlaceSettings setLiquidSettings(LiquidSettings liquidSettings) {
        this.liquidSettings = liquidSettings;
        return this;
    }
    public StructurePlaceSettings setKnownShape(boolean knownShape) {
        this.knownShape = knownShape;
        return this;
    }
    public StructurePlaceSettings clearProcessors() {
        processors.clear();
        return this;
    }
    public StructurePlaceSettings addProcessor(StructureProcessor processor) {
        processors.add(processor);
        return this;
    }
    public StructurePlaceSettings popProcessor(StructureProcessor processor) {
        processors.remove(processor);
        return this;
    }
    public Mirror getMirror() {
        return mirror;
    }
    public Rotation getRotation() {
        return rotation;
    }
    public BlockPos getRotationPivot() {
        return rotationPivot;
    }
    public RandomSource getRandom(BlockPos pos) {
        if (random != null) {
            return random;
        }
        return RandomSource.create(pos == null ? System.currentTimeMillis() : net.minecraft.util.Mth.getSeed(pos));
    }
    public boolean isIgnoreEntities() {
        return ignoreEntities;
    }
    public BoundingBox getBoundingBox() {
        return boundingBox;
    }
    public boolean getKnownShape() {
        return knownShape;
    }
    public List<StructureProcessor> getProcessors() {
        return processors;
    }
    public boolean shouldApplyWaterlogging() {
        return liquidSettings == LiquidSettings.APPLY_WATERLOGGING;
    }
    public StructureTemplate.Palette getRandomPalette(List<StructureTemplate.Palette> palettes, BlockPos pos) {
        int count = palettes.size();
        if (count == 0) {
            throw new IllegalStateException("No palettes");
        }
        return palettes.get(getRandom(pos).nextInt(count));
    }
    public StructurePlaceSettings setFinalizeEntities(boolean finalizeEntities) {
        this.finalizeEntities = finalizeEntities;
        return this;
    }
    public boolean shouldFinalizeEntities() {
        return finalizeEntities;
    }
}
