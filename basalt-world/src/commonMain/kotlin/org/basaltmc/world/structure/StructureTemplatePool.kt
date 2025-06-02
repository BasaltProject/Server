package org.basaltmc.world.structure

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.world.Chunk

/** Loads a real NBT structure piece by its `minecraft:...` location string, or `null` if unavailable. */
public typealias TemplateLoader = (String) -> StructureTemplate?

/** // parity: net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool.Projection. */
public enum class StructureProjection {
    TERRAIN_MATCHING,
    RIGID,
}

/** // parity: net.minecraft.util.Util.shuffle: Fisher-Yates from the top, java.util.Random-compatible swap order. */
public fun <T> shuffledCopy(
    list: List<T>,
    random: WorldgenRandom,
): List<T> {
    val copy = list.toMutableList()
    var i = copy.size
    while (i > 1) {
        val swapTo = random.nextInt(i)
        val tmp = copy[i - 1]
        copy[i - 1] = copy[swapTo]
        copy[swapTo] = tmp
        i--
    }
    return copy
}

/** // parity: net.minecraft.world.level.block.Rotation.getShuffled / getRandom. */
public object StructureRotations {
    private val ALL = StructureRotation.entries.toList()

    public fun random(random: WorldgenRandom): StructureRotation = ALL[random.nextInt(ALL.size)]

    public fun shuffled(random: WorldgenRandom): List<StructureRotation> = shuffledCopy(ALL, random)
}

/** // parity: net.minecraft.world.level.levelgen.structure.pools.StructurePoolElement, narrowed to the element kinds real Plains village pools use, no mirror (vanilla's jigsaw runtime never mirrors pieces; pre-mirrored variants are baked as separate NBT files). */
public sealed class StructurePoolElement(
    public val projection: StructureProjection,
) {
    public open val groundLevelDelta: Int get() = 1

    public abstract fun getBoundingBox(
        loader: TemplateLoader,
        position: StructurePos,
        rotation: StructureRotation,
    ): StructureBoundingBox

    public abstract fun getShuffledJigsawBlocks(
        loader: TemplateLoader,
        position: StructurePos,
        rotation: StructureRotation,
        random: WorldgenRandom,
    ): List<PlacedJigsaw>

    public abstract fun place(
        loader: TemplateLoader,
        chunk: Chunk,
        position: StructurePos,
        rotation: StructureRotation,
        clipTo: StructureBoundingBox,
        random: WorldgenRandom,
    )
}

/** // parity: net.minecraft.world.level.levelgen.structure.pools.EmptyPoolElement. */
public object EmptyPoolElement : StructurePoolElement(StructureProjection.RIGID) {
    override val groundLevelDelta: Int = 0

    private val EMPTY_BOX = StructureBoundingBox(0, 0, 0, -1, -1, -1)

    override fun getBoundingBox(
        loader: TemplateLoader,
        position: StructurePos,
        rotation: StructureRotation,
    ): StructureBoundingBox = EMPTY_BOX

    override fun getShuffledJigsawBlocks(
        loader: TemplateLoader,
        position: StructurePos,
        rotation: StructureRotation,
        random: WorldgenRandom,
    ): List<PlacedJigsaw> = emptyList()

    override fun place(
        loader: TemplateLoader,
        chunk: Chunk,
        position: StructurePos,
        rotation: StructureRotation,
        clipTo: StructureBoundingBox,
        random: WorldgenRandom,
    ) {
    }
}

/**
 * // parity: SinglePoolElement and LegacySinglePoolElement. [ignoreAir] captures their meaningful
 * placement difference: legacy pieces use STRUCTURE_AND_AIR while modern single pieces only ignore
 * structure blocks. Every real Plains village piece is legacy; every Bastion piece is modern.
 */
public class LegacySinglePoolElement(
    public val location: String,
    public val processors: StructureProcessorList,
    projection: StructureProjection,
    public val ignoreAir: Boolean = true,
) : StructurePoolElement(projection) {
    override fun getBoundingBox(
        loader: TemplateLoader,
        position: StructurePos,
        rotation: StructureRotation,
    ): StructureBoundingBox {
        val template = loader(location) ?: return StructureBoundingBox(0, 0, 0, -1, -1, -1)
        return template.getBoundingBox(position, rotation, StructurePos(0, 0, 0), StructureMirror.NONE)
    }

    override fun getShuffledJigsawBlocks(
        loader: TemplateLoader,
        position: StructurePos,
        rotation: StructureRotation,
        random: WorldgenRandom,
    ): List<PlacedJigsaw> {
        val template = loader(location) ?: return emptyList()
        val jigsaws = template.jigsawBlocks(position, rotation, StructureMirror.NONE, StructurePos(0, 0, 0))
        return shuffledCopy(jigsaws, random).sortedByDescending(PlacedJigsaw::selectionPriority)
    }

    override fun place(
        loader: TemplateLoader,
        chunk: Chunk,
        position: StructurePos,
        rotation: StructureRotation,
        clipTo: StructureBoundingBox,
        random: WorldgenRandom,
    ) {
        val template = loader(location) ?: return
        template.placeInto(
            chunk,
            position,
            rotation,
            StructureMirror.NONE,
            StructurePos(0, 0, 0),
            clipTo,
            processors,
            random,
            ignoreStructureBlocks = true,
            ignoreAir = ignoreAir,
        )
    }
}

/** // parity: net.minecraft.world.level.levelgen.structure.pools.ListPoolElement: several sub-elements placed together as one unit. Not used by any real Plains village pool entry, ported for completeness. */
public class ListPoolElement(
    public val elements: List<StructurePoolElement>,
    projection: StructureProjection,
) : StructurePoolElement(projection) {
    override fun getBoundingBox(
        loader: TemplateLoader,
        position: StructurePos,
        rotation: StructureRotation,
    ): StructureBoundingBox {
        val boxes = elements.map { it.getBoundingBox(loader, position, rotation) }
        return boxes.reduceOrNull {
            a,
            b,
            ->
            StructureBoundingBox.fromCorners(StructurePos(a.minX, a.minY, a.minZ), StructurePos(b.maxX, b.maxY, b.maxZ))
        }
            ?: StructureBoundingBox(0, 0, 0, -1, -1, -1)
    }

    override fun getShuffledJigsawBlocks(
        loader: TemplateLoader,
        position: StructurePos,
        rotation: StructureRotation,
        random: WorldgenRandom,
    ): List<PlacedJigsaw> =
        elements.firstOrNull()?.getShuffledJigsawBlocks(loader, position, rotation, random) ?: emptyList()

    override fun place(
        loader: TemplateLoader,
        chunk: Chunk,
        position: StructurePos,
        rotation: StructureRotation,
        clipTo: StructureBoundingBox,
        random: WorldgenRandom,
    ) {
        for (element in elements) element.place(loader, chunk, position, rotation, clipTo, random)
    }
}

/**
 * // parity: net.minecraft.world.level.levelgen.structure.pools.FeaturePoolElement: a placed
 * worldgen feature (a tree, a block pile, a small patch) as a village piece, used by the real
 * desert/savanna/snowy/taiga `decor`/`trees` pools. Zero-sized like the real one
 * (`getSize` returns `Vec3i.ZERO`), with no jigsaw blocks of its own.
 */
public class FeaturePoolElement(
    public val feature: String,
    projection: StructureProjection,
) : StructurePoolElement(projection) {
    override fun getBoundingBox(
        loader: TemplateLoader,
        position: StructurePos,
        rotation: StructureRotation,
    ): StructureBoundingBox =
        StructureBoundingBox(position.x, position.y, position.z, position.x, position.y, position.z)

    override fun getShuffledJigsawBlocks(
        loader: TemplateLoader,
        position: StructurePos,
        rotation: StructureRotation,
        random: WorldgenRandom,
    ): List<PlacedJigsaw> = emptyList()

    override fun place(
        loader: TemplateLoader,
        chunk: Chunk,
        position: StructurePos,
        rotation: StructureRotation,
        clipTo: StructureBoundingBox,
        random: WorldgenRandom,
    ) {
        org.basaltmc.world.terrain.decoration.VillageFeatures.place(
            feature,
            chunk,
            position.x,
            position.y,
            position.z,
            random,
        )
    }
}

/** // parity: net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool. */
public class StructureTemplatePool(
    public val fallback: String,
    weightedElements: List<Pair<StructurePoolElement, Int>>,
) {
    private val templates: List<StructurePoolElement> =
        weightedElements.flatMap { (element, weight) -> List(weight) { element } }

    public fun getRandomTemplate(random: WorldgenRandom): StructurePoolElement =
        if (templates.isEmpty()) EmptyPoolElement else templates[random.nextInt(templates.size)]

    public fun getShuffledTemplates(random: WorldgenRandom): List<StructurePoolElement> =
        shuffledCopy(templates, random)

    public fun size(): Int = templates.size
}
