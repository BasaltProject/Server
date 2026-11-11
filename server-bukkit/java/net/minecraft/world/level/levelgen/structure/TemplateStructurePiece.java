// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import net.minecraft.core.registries.Registries;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructurePlaceSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
public abstract class TemplateStructurePiece extends StructurePiece {
    protected String templateName;
    protected StructureTemplate template;
    protected StructurePlaceSettings placeSettings;
    protected BlockPos templatePosition;
    public TemplateStructurePiece(StructurePieceType type, int genDepth, StructureTemplateManager structureTemplateManager, Identifier templateLocation, String templateName, StructurePlaceSettings placeSettings, BlockPos position) {
        super(type, genDepth, structureTemplateManager.getOrCreate(templateLocation).getBoundingBox(placeSettings, position));
        this.setOrientation(Direction.NORTH);
        this.templateName = templateName;
        this.templatePosition = position;
        this.template = structureTemplateManager.getOrCreate(templateLocation);
        this.placeSettings = placeSettings;
    }
    public TemplateStructurePiece(StructurePieceType type, CompoundTag tag, StructureTemplateManager structureTemplateManager, Function<Identifier, StructurePlaceSettings> structurePlaceSettingsSupplier) {
        super(type, tag);
        this.setOrientation(Direction.NORTH);
        this.templateName = tag.getStringOr("Template", "");
        this.templatePosition = new BlockPos(tag.getIntOr("TPX", 0), tag.getIntOr("TPY", 0), tag.getIntOr("TPZ", 0));
        Identifier location = makeTemplateLocation();
        this.template = structureTemplateManager.getOrCreate(location);
        this.placeSettings = structurePlaceSettingsSupplier.apply(location);
        this.boundingBox = this.template.getBoundingBox(this.placeSettings, this.templatePosition);
    }
    protected Identifier makeTemplateLocation() {
        return Identifier.parse(templateName);
    }
    protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
        tag.putInt("TPX", templatePosition.getX());
        tag.putInt("TPY", templatePosition.getY());
        tag.putInt("TPZ", templatePosition.getZ());
        tag.putString("Template", templateName);
    }
    public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
        placeSettings.setBoundingBox(chunkBB);
        boundingBox = template.getBoundingBox(placeSettings, templatePosition);
        if (!template.placeInWorld(level, templatePosition, referencePos, placeSettings, random, 2)) {
            return;
        }
        for (StructureTemplate.StructureBlockInfo marker : template.filterBlocks(templatePosition, placeSettings, Blocks.STRUCTURE_BLOCK)) {
            if (marker.nbt() != null
                && marker.nbt().read("mode", net.minecraft.world.level.block.state.properties.StructureMode.LEGACY_CODEC).orElseThrow()
                    == net.minecraft.world.level.block.state.properties.StructureMode.DATA) {
                handleDataMarker(marker.nbt().getStringOr("metadata", ""), marker.pos(), level, random, chunkBB);
            }
        }
        for (StructureTemplate.StructureBlockInfo jigsaw : template.filterBlocks(templatePosition, placeSettings, Blocks.JIGSAW)) {
            if (jigsaw.nbt() == null) {
                continue;
            }
            String finalState = jigsaw.nbt().getStringOr("final_state", "minecraft:air");
            BlockState target = Blocks.AIR.defaultBlockState();
            try {
                target = net.minecraft.commands.arguments.blocks.BlockStateParser.parseForBlock(level.holderLookup(Registries.BLOCK), finalState, true).blockState();
            } catch (com.mojang.brigadier.exceptions.CommandSyntaxException bad) {
                org.slf4j.LoggerFactory.getLogger(TemplateStructurePiece.class).error("Error while parsing blockstate {} in jigsaw block @ {}", finalState, jigsaw.pos());
            }
            level.setBlockAndUpdate(jigsaw.pos(), target);
        }
    }
    protected abstract void handleDataMarker(String markerId, BlockPos position, ServerLevelAccessor level, RandomSource random, BoundingBox chunkBB);
    public void move(int dx, int dy, int dz) {
        super.move(dx, dy, dz);
        templatePosition = templatePosition.offset(dx, dy, dz);
    }
    public Rotation getRotation() {
        return placeSettings.getRotation();
    }
    public StructureTemplate template() {
        return template;
    }
    public BlockPos templatePosition() {
        return templatePosition;
    }
    public StructurePlaceSettings placeSettings() {
        return placeSettings;
    }
}
