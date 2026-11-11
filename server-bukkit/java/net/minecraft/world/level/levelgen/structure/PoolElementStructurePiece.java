// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.pools.JigsawJunction;
import net.minecraft.world.level.levelgen.structure.pools.StructurePoolElement;
import net.minecraft.world.level.levelgen.structure.templatesystem.LiquidSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
public class PoolElementStructurePiece extends StructurePiece {
    private int groundLevelDelta;
    private final List<JigsawJunction> junctions = new java.util.ArrayList<>();
    private StructureTemplateManager structureTemplateManager;
    private LiquidSettings liquidSettings;
    protected StructurePoolElement element;
    protected BlockPos position;
    protected Rotation rotation;
    public PoolElementStructurePiece(StructureTemplateManager structureTemplateManager, StructurePoolElement element, BlockPos position, int groundLevelDelta, Rotation rotation, BoundingBox boundingBox, LiquidSettings liquidSettings) {
        super(StructurePieceType.JIGSAW, 0, boundingBox);
        this.element = element;
        this.rotation = rotation;
        this.position = position;
        this.structureTemplateManager = structureTemplateManager;
        this.groundLevelDelta = groundLevelDelta;
        this.liquidSettings = liquidSettings;
    }
    public PoolElementStructurePiece(StructurePieceSerializationContext context, CompoundTag tag) {
        super(StructurePieceType.JIGSAW, tag);
        structureTemplateManager = context.structureTemplateManager();
        groundLevelDelta = tag.getIntOr("ground_level_delta", 0);
        this.position = new BlockPos(tag.getIntOr("PosX", 0), tag.getIntOr("PosY", 0), tag.getIntOr("PosZ", 0));

        com.mojang.serialization.DynamicOps<net.minecraft.nbt.Tag> ops = context.registryAccess().createSerializationContext(net.minecraft.nbt.NbtOps.INSTANCE);
        this.element = tag.read("pool_element", StructurePoolElement.CODEC, ops).orElseThrow(() -> new IllegalStateException("Invalid pool element found"));
        this.rotation = tag.read("rotation", Rotation.LEGACY_CODEC).orElseThrow();
        this.boundingBox = element.getBoundingBox(structureTemplateManager, position, rotation);
        net.minecraft.nbt.ListTag saved = tag.getListOrEmpty("junctions");
        junctions.clear();
        saved.forEach(entry -> junctions.add(JigsawJunction.deserialize(new com.mojang.serialization.Dynamic<>(ops, entry))));
        this.liquidSettings = tag.read("liquid_settings", LiquidSettings.CODEC).orElse(net.minecraft.world.level.levelgen.structure.structures.JigsawStructure.DEFAULT_LIQUID_SETTINGS);
    }
    protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
        tag.putInt("ground_level_delta", groundLevelDelta);
        tag.putInt("PosX", position.getX());
        tag.putInt("PosY", position.getY());
        tag.putInt("PosZ", position.getZ());
        com.mojang.serialization.DynamicOps<net.minecraft.nbt.Tag> ops = context.registryAccess().createSerializationContext(net.minecraft.nbt.NbtOps.INSTANCE);
        tag.store("pool_element", StructurePoolElement.CODEC, ops, element);
        tag.store("rotation", Rotation.LEGACY_CODEC, rotation);
        net.minecraft.nbt.ListTag saved = new net.minecraft.nbt.ListTag();
        for (JigsawJunction junction : junctions) {
            saved.add(junction.serialize(ops).getValue());
        }
        tag.put("junctions", saved);
        if (liquidSettings != net.minecraft.world.level.levelgen.structure.structures.JigsawStructure.DEFAULT_LIQUID_SETTINGS) {
            tag.store("liquid_settings", LiquidSettings.CODEC, ops, liquidSettings);
        }
    }
    public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
        place(level, structureManager, generator, random, chunkBB, referencePos, false);
    }
    public void place(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, BlockPos referencePos, boolean keepJigsaws) {
        element.place(structureTemplateManager, level, structureManager, generator, position, referencePos, rotation, chunkBB, random, liquidSettings, keepJigsaws);
    }
    public void move(int dx, int dy, int dz) {
        super.move(dx, dy, dz);
        position = position.offset(dx, dy, dz);
    }
    public Rotation getRotation() {
        return rotation;
    }
    public String toString() {
        return super.toString();
    }
    public StructurePoolElement getElement() {
        return element;
    }
    public BlockPos getPosition() {
        return position;
    }
    public int getGroundLevelDelta() {
        return groundLevelDelta;
    }
    public void addJunction(JigsawJunction junction) {
        junctions.add(junction);
    }
    public List<JigsawJunction> getJunctions() {
        return junctions;


    }
}
