// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.npc.villager;
import com.mojang.serialization.Codec;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
public record VillagerData(Holder<VillagerType> type, Holder<VillagerProfession> profession, int level) {
    public static final ResourceKey<VillagerType> DEFAULT_TYPE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.VILLAGER_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("default_type"));
    public static final int MIN_VILLAGER_LEVEL = 1;
    public static final int MAX_VILLAGER_LEVEL = 5;
    public static final Codec<VillagerData> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.npc.villager.VillagerData.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, VillagerData> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.npc.villager.VillagerData.class, "STREAM_CODEC");
    public VillagerData(Holder<VillagerType> type, Holder<VillagerProfession> profession, int level) {
        this.type = type;
        this.profession = profession;
        this.level = Math.max(1, level);
    }
    public VillagerData withType(Holder<VillagerType> type) {
        return new VillagerData(type, profession, level);
    }
    public VillagerData withType(HolderGetter.Provider registries, ResourceKey<VillagerType> type) {
        return new VillagerData(registries.lookupOrThrow(net.minecraft.core.registries.Registries.VILLAGER_TYPE).getOrThrow(type), profession, level);
    }
    public VillagerData withProfession(Holder<VillagerProfession> profession) {
        return new VillagerData(this.type, profession, level);
    }
    public VillagerData withProfession(HolderGetter.Provider registries, ResourceKey<VillagerProfession> profession) {
        return new VillagerData(this.type, registries.lookupOrThrow(net.minecraft.core.registries.Registries.VILLAGER_PROFESSION).getOrThrow(profession), level);
    }
    public VillagerData withLevel(int level) {
        return new VillagerData(type, profession, level);
    }
    public static int getMinXpPerLevel(int level) {
        return canLevelUp(level - 1) ? kXpThresholds[level - 1] : 0;
    }
    public static int getMaxXpPerLevel(int level) {
        return canLevelUp(level) ? kXpThresholds[level] : 0;
    }
    public static boolean canLevelUp(int currentLevel) {
        return currentLevel >= 1 && currentLevel < 5;
    }
    private static final int[] kXpThresholds = {0, 10, 70, 150, 250};
}
