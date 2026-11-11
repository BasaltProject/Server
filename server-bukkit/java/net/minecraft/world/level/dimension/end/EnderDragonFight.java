// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.dimension.end;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Optional;
import java.util.UUID;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityReference;
import net.minecraft.world.entity.boss.enderdragon.EndCrystal;
import net.minecraft.world.entity.boss.enderdragon.EnderDragon;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;
public class EnderDragonFight extends SavedData {
    public static final int TIME_BETWEEN_PLAYER_SCANS = 20;
    public static final int ARENA_TICKET_LEVEL = 9;
    public static final int DRAGON_SPAWN_Y = 128;
    public static final Codec<EnderDragonFight> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.dimension.end.EnderDragonFight.class, "CODEC");
    public static final SavedDataType<EnderDragonFight> TYPE = new SavedDataType<>(net.minecraft.resources.Identifier.withDefaultNamespace("ender_dragon_fight"), EnderDragonFight::createDefault, CODEC, net.minecraft.util.datafix.DataFixTypes.SAVED_DATA_ENDER_DRAGON_FIGHT);
    public static EnderDragonFight createDefault() {
        return new EnderDragonFight((org.bukkit.World) null);
    }
    private org.bukkit.World world;

    /** The fight of an end world, read through its Bukkit dragon battle. */
    public EnderDragonFight(org.bukkit.World world) {
        this.world = world;
    }

    private org.bukkit.boss.DragonBattle battle() {
        return world == null ? null : world.getEnderDragonBattle();
    }

    public EnderDragonFight(boolean needsStateScanning, boolean dragonKilled, boolean previouslyKilled, Optional<DragonRespawnStage> respawnStage, int respawnTime, Optional<UUID> dragonUUID, Optional<BlockPos> exitPortalLocation, List<Integer> gateways, List<EntityReference<EndCrystal>> respawnCrystals) {
        
    }
    public static BlockPos getPodiumLocation(BlockPos offset) {
        return offset.offset(0, 0, 0);
    }
    public void init(ServerLevel level, long seed, BlockPos origin) {
        this.world = level.getWorld();
    }
    public void skipArenaLoadedCheck() {
    }
    public void tick() {
    }
    protected void setRespawnStage(DragonRespawnStage stage) {
    }
    public void setDragonKilled(EnderDragon dragon) {
    }
    public void removeAllGateways() {
    }
    public void updateDragon(EnderDragon dragon) {
    }
    public int aliveCrystals() {
        return world == null ? 0 : world.getEntitiesByClass(org.bukkit.entity.EnderCrystal.class).size();
    }
    public void onCrystalDestroyed(EndCrystal crystal, DamageSource source) {
    }
    public boolean hasPreviouslyKilledDragon() {
        org.bukkit.boss.DragonBattle battle = battle();
        return battle != null && battle.hasBeenPreviouslyKilled();
    }
    public void tryRespawn() {
        org.bukkit.boss.DragonBattle battle = battle();
        if (battle != null) {
            battle.initiateRespawn();
        }
    }
    public void resetSpikeCrystals() {
        org.bukkit.boss.DragonBattle battle = battle();
        if (battle != null) {
            battle.resetCrystals();
        }
    }
    public UUID dragonUUID() {
        org.bukkit.boss.DragonBattle battle = battle();
        return battle == null || battle.getEnderDragon() == null ? null : battle.getEnderDragon().getUniqueId();
    }
}
