// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pools;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.Rotation;
import com.google.common.collect.ImmutableList;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.function.Function;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessor;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
public class StructureTemplatePool {
    private final List<Pair<StructurePoolElement, Integer>> rawTemplates;
    private final java.util.ArrayList<StructurePoolElement> templates = new java.util.ArrayList<>();
    private final Holder<StructureTemplatePool> fallback;
    private int maxSize = Integer.MIN_VALUE;
    public static final Codec<StructureTemplatePool> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool.class, "DIRECT_CODEC");
    public static final Codec<Holder<StructureTemplatePool>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool.class, "CODEC");
    public StructureTemplatePool(Holder<StructureTemplatePool> fallback, List<Pair<StructurePoolElement, Integer>> templates) {
        this.rawTemplates = templates;
        for (Pair<StructurePoolElement, Integer> def : templates) {
            for (int i = 0; i < def.getSecond(); i++) {
                this.templates.add(def.getFirst());
            }
        }
        this.fallback = fallback;
    }
    public StructureTemplatePool(Holder<StructureTemplatePool> fallback, List<Pair<Function<Projection, ? extends StructurePoolElement>, Integer>> templates, Projection projection) {
        this.rawTemplates = new java.util.ArrayList<>();
        for (Pair<Function<Projection, ? extends StructurePoolElement>, Integer> def : templates) {
            StructurePoolElement built = def.getFirst().apply(projection);
            this.rawTemplates.add(Pair.of(built, def.getSecond()));
            for (int i = 0; i < def.getSecond(); i++) {
                this.templates.add(built);
            }
        }
        this.fallback = fallback;
    }
    public int getMaxSize(StructureTemplateManager manager) {
        if (maxSize == Integer.MIN_VALUE) {
            maxSize = templates.stream().filter(t -> t != EmptyPoolElement.INSTANCE)
                .mapToInt(t -> t.getBoundingBox(manager, BlockPos.ZERO, Rotation.NONE).getYSpan()).max().orElse(0);
        }
        return maxSize;
    }
    public List<Pair<StructurePoolElement, Integer>> getTemplates() {
        return rawTemplates;
    }
    public Holder<StructureTemplatePool> getFallback() {
        return fallback;
    }
    public StructurePoolElement getRandomTemplate(RandomSource random) {
        return templates.isEmpty() ? EmptyPoolElement.INSTANCE : templates.get(random.nextInt(templates.size()));
    }
    public List<StructurePoolElement> getShuffledTemplates(RandomSource random) {
        return net.minecraft.util.Util.shuffledCopy(new it.unimi.dsi.fastutil.objects.ObjectArrayList<>(templates), random);
    }
    public int size() {
        return templates.size();
    }
    public static enum Projection implements StringRepresentable {
        TERRAIN_MATCHING("terrain_matching", ImmutableList.of(new net.minecraft.world.level.levelgen.structure.templatesystem.GravityProcessor(net.minecraft.world.level.levelgen.Heightmap.Types.WORLD_SURFACE_WG, -1))),
        RIGID("rigid", ImmutableList.of());
        private final String name;
        private final ImmutableList<StructureProcessor> processors;
        private Projection(String name, ImmutableList<StructureProcessor> processors) {
            this.name = name;
            this.processors = processors;
        }
        public static final StringRepresentable.EnumCodec<Projection> CODEC = net.minecraft.util.StringRepresentable.fromEnum(net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool.Projection::values);
        public String getName() {
            return name;
        }
        public static Projection byName(String name) {
            return CODEC.byName(name);
        }
        public ImmutableList<StructureProcessor> getProcessors() {
            return processors;
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}
