// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.raid;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import java.util.UUID;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.component.DataComponentPatch;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.Difficulty;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.SpawnPlacementType;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.core.component.DataComponents;
import net.minecraft.network.chat.Component;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.Rarity;
import net.minecraft.world.item.component.TooltipDisplay;
import net.minecraft.world.level.block.entity.BannerPattern;
import net.minecraft.world.level.block.entity.BannerPatternLayers;
import net.minecraft.world.level.block.entity.BannerPatterns;
public class Raid {
    public static final SpawnPlacementType RAVAGER_SPAWN_PLACEMENT_TYPE = net.minecraft.world.entity.SpawnPlacements.getPlacementType(net.minecraft.world.entity.EntityTypes.RAVAGER);
    public static final MapCodec<Raid> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.entity.raid.Raid.class, "MAP_CODEC");
    public static final int VILLAGE_RADIUS_BUFFER = 16;
    public static final int MAX_NO_ACTION_TIME = 2400;
    public static final int MAX_CELEBRATION_TICKS = 600;
    public static final int DEFAULT_MAX_RAID_OMEN_LEVEL = 5;
    public static final int VALID_RAID_RADIUS_SQR = 9216;
    public static final int RAID_REMOVAL_THRESHOLD_SQR = 12544;

    private final BlockPos center;
    private final org.bukkit.Raid bukkit;
    private int standaloneOmenLevel;

    public Raid(BlockPos center, Difficulty difficulty) {
        this.center = center;
        this.bukkit = null;
    }

    /** A raid of the native world, read through the Bukkit view of it. */
    public Raid(org.bukkit.Raid bukkit) {
        this.bukkit = bukkit;
        org.bukkit.Location at = bukkit.getLocation();
        this.center = new BlockPos(at.getBlockX(), at.getBlockY(), at.getBlockZ());
    }

    public int nativeId() {
        return bukkit == null ? -1 : bukkit.getId();
    }

    private org.bukkit.Raid.RaidStatus status() {
        return bukkit == null ? org.bukkit.Raid.RaidStatus.ONGOING : bukkit.getStatus();
    }

    public boolean isOver() {
        return status() != org.bukkit.Raid.RaidStatus.ONGOING;
    }
    public boolean isBetweenWaves() {
        return bukkit != null && isActive() && isStarted() && getTotalRaidersAlive() == 0 && getGroupsSpawned() > 0;
    }
    public boolean hasFirstWaveSpawned() {
        return getGroupsSpawned() > 0;
    }
    public boolean isStopped() {
        return status() == org.bukkit.Raid.RaidStatus.STOPPED;
    }
    public boolean isVictory() {
        return status() == org.bukkit.Raid.RaidStatus.VICTORY;
    }
    public boolean isLoss() {
        return status() == org.bukkit.Raid.RaidStatus.LOSS;
    }
    public float getTotalHealth() {
        return bukkit == null ? 0.0F : bukkit.getTotalHealth();
    }
    public Set<Raider> getAllRaiders() {
        Set<Raider> out = new java.util.LinkedHashSet<>();
        if (bukkit != null) {
            for (org.bukkit.entity.Raider each : bukkit.getRaiders()) {
                if (((org.bukkit.craftbukkit.entity.CraftEntity) each).getHandle() instanceof Raider handle) {
                    out.add(handle);
                }
            }
        }
        return out;
    }
    public boolean isStarted() {
        return bukkit != null && bukkit.isStarted();
    }
    public int getGroupsSpawned() {
        return bukkit == null ? 0 : bukkit.getSpawnedGroups();
    }
    public int getMaxRaidOmenLevel() {
        return DEFAULT_MAX_RAID_OMEN_LEVEL;
    }
    public int getRaidOmenLevel() {
        return bukkit == null ? standaloneOmenLevel : bukkit.getBadOmenLevel();
    }
    public void setRaidOmenLevel(int raidOmenLevel) {
        if (bukkit == null) {
            standaloneOmenLevel = raidOmenLevel;
        } else {
            bukkit.setBadOmenLevel(raidOmenLevel);
        }
    }
    public boolean absorbRaidOmen(ServerPlayer player) {
        return false;
    }
    private String worldName() {
        return bukkit.getLocation().getWorld().getName();
    }

    private boolean control(String action, int first, int second) {
        return bukkit != null && org.basaltmc.bukkit.WorldEnvNatives.raidControl(worldName(), bukkit.getId(), action, first, second, 0, 0, 0) >= 0;
    }

    public void stop() {
        control("stop", 0, 0);
    }
    public void tick(ServerLevel level) {
    }
    public void joinRaid(ServerLevel level, int groupNumber, Raider raider, BlockPos pos, boolean exists) {
        control("join", raider.getId(), groupNumber);
    }
    public void updateBossbar() {
    }
    public float getHealthOfLivingRaiders() {
        return getTotalHealth();
    }
    public int getTotalRaidersAlive() {
        return bukkit == null ? 0 : bukkit.getRaiders().size();
    }
    public void removeFromRaid(ServerLevel level, Raider raider, boolean removeFromTotalHealth) {
    }
    public static DataComponentPatch getBannerComponentPatch(HolderGetter<BannerPattern> patternGetter) {
        BannerPatternLayers layers = new BannerPatternLayers.Builder()
            .addIfRegistered(patternGetter, BannerPatterns.RHOMBUS_MIDDLE, DyeColor.CYAN)
            .addIfRegistered(patternGetter, BannerPatterns.STRIPE_BOTTOM, DyeColor.LIGHT_GRAY)
            .addIfRegistered(patternGetter, BannerPatterns.STRIPE_CENTER, DyeColor.GRAY)
            .addIfRegistered(patternGetter, BannerPatterns.BORDER, DyeColor.LIGHT_GRAY)
            .addIfRegistered(patternGetter, BannerPatterns.STRIPE_MIDDLE, DyeColor.BLACK)
            .addIfRegistered(patternGetter, BannerPatterns.HALF_HORIZONTAL, DyeColor.LIGHT_GRAY)
            .addIfRegistered(patternGetter, BannerPatterns.CIRCLE_MIDDLE, DyeColor.LIGHT_GRAY)
            .addIfRegistered(patternGetter, BannerPatterns.BORDER, DyeColor.BLACK).build();
        return DataComponentPatch.builder().set(DataComponents.BANNER_PATTERNS, layers)
            .set(DataComponents.TOOLTIP_DISPLAY, TooltipDisplay.DEFAULT.withHidden(DataComponents.BANNER_PATTERNS, true))
            .set(DataComponents.ITEM_NAME, Component.translatable("block.minecraft.ominous_banner")).set(DataComponents.RARITY, Rarity.UNCOMMON)
            .build();
    }
    public static ItemStackTemplate getOminousBannerTemplate(HolderGetter<BannerPattern> patternGetter) {
        return new ItemStackTemplate(Items.BANNER.white(), getBannerComponentPatch(patternGetter));
    }
    public static ItemStack getOminousBannerInstance(HolderGetter<BannerPattern> patternGetter) {
        return getOminousBannerTemplate(patternGetter).create();
    }
    public Raider getLeader(int wave) {
        if (bukkit == null) {
            return null;
        }
        for (String row : org.basaltmc.bukkit.WorldEnvNatives.raids(worldName())) {
            String[] parts = row.split("\\|", -1);
            if (Integer.parseInt(parts[0]) != bukkit.getId() || parts.length < 12) {
                continue;
            }
            for (String leader : parts[11].isEmpty() ? new String[0] : parts[11].split(",")) {
                String[] pair = leader.split(":");
                if (Integer.parseInt(pair[0]) == wave && ((org.bukkit.craftbukkit.CraftWorld) bukkit.getLocation().getWorld()).getHandle().getEntity(Integer.parseInt(pair[1])) instanceof Raider found) {
                    return found;
                }
            }
        }
        return null;
    }
    public boolean addWaveMob(ServerLevel level, int wave, Raider raider, boolean updateHealth) {
        return control("join", raider.getId(), wave);
    }
    public void setLeader(int wave, Raider raider) {
        control("set_leader", wave, raider.getId());
    }
    public void removeLeader(int wave) {
        control("remove_leader", wave, 0);
    }
    public BlockPos getCenter() {
        return center;
    }
    public boolean isActive() {
        return status() == org.bukkit.Raid.RaidStatus.ONGOING;
    }
    public int getNumGroups(Difficulty difficulty) {
        return switch (difficulty.name()) {
            case "EASY" -> 3;
            case "NORMAL" -> 5;
            case "HARD" -> 7;
            default -> 0;
        };
    }
    public float getEnchantOdds() {
        int omen = getRaidOmenLevel();
        return omen == 2 ? 0.1F : omen == 3 ? 0.25F : 0.0F;
    }
    public void addHeroOfTheVillage(Entity killer) {
    }
    static enum RaidStatus implements StringRepresentable {
        ONGOING, VICTORY, LOSS, STOPPED;
        public static final Codec<RaidStatus> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.raid.Raid.RaidStatus.class, "CODEC");
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
    static enum RaiderType {
        VINDICATOR, EVOKER, PILLAGER, WITCH, RAVAGER;
    }
}
