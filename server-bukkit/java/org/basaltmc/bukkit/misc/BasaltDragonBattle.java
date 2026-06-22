package org.basaltmc.bukkit.misc;

import java.util.Collection;
import java.util.List;
import org.basaltmc.bukkit.LeftoverNatives;
import org.basaltmc.bukkit.MiscNatives;
import org.basaltmc.bukkit.Unsupported;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.World;
import org.bukkit.boss.BossBar;
import org.bukkit.boss.DragonBattle;
import org.bukkit.entity.EnderCrystal;
import org.bukkit.entity.EnderDragon;
import org.bukkit.entity.Entity;

/** The fight of an End world as the server keeps it; what only the fight's own ticking can do is not offered. */
public final class BasaltDragonBattle implements DragonBattle {
    private final World world;

    private BasaltDragonBattle(World world) {
        this.world = world;
    }

    public static DragonBattle of(World world) {
        return MiscNatives.dragonFight(world.getName()) == null ? null : new BasaltDragonBattle(world);
    }

    private String[] facts() {
        String[] facts = MiscNatives.dragonFight(world.getName());
        if (facts == null) {
            throw new IllegalStateException("The world has no dragon fight");
        }
        return facts;
    }

    @Override
    public EnderDragon getEnderDragon() {
        String uuid = facts()[4];
        Entity found = uuid.isEmpty() ? null : Bukkit.getEntity(java.util.UUID.fromString(uuid));
        return found instanceof EnderDragon dragon ? dragon : null;
    }

    @Override
    public BossBar getBossBar() {
        throw Unsupported.hit("DragonBattle#getBossBar");
    }

    @Override
    public Location getEndPortalLocation() {
        String portal = facts()[1];
        if (portal.isEmpty()) {
            return null;
        }
        String[] at = portal.split(",");
        return new Location(world, Integer.parseInt(at[0]), Integer.parseInt(at[1]), Integer.parseInt(at[2]));
    }

    @Override
    public boolean generateEndPortal(boolean withPortals) {
        return LeftoverNatives.dragonAction(world.getName(), "portal", withPortals ? 1 : 0);
    }

    @Override
    public boolean hasBeenPreviouslyKilled() {
        return "1".equals(facts()[0]);
    }

    @Override
    public void setPreviouslyKilled(boolean previouslyKilled) {
        MiscNatives.setDragonPreviouslyKilled(world.getName(), previouslyKilled);
    }

    @Override
    public void initiateRespawn() {
        LeftoverNatives.dragonAction(world.getName(), "respawn", 0);
    }

    @Override
    public boolean initiateRespawn(Collection<EnderCrystal> crystals) {
        LeftoverNatives.dragonAction(world.getName(), "respawn", 0);
        return getRespawnPhase() != RespawnPhase.NONE;
    }

    @Override
    public RespawnPhase getRespawnPhase() {
        return switch (facts()[2]) {
            case "start" -> RespawnPhase.START;
            case "preparing_to_summon_pillars" -> RespawnPhase.PREPARING_TO_SUMMON_PILLARS;
            case "summoning_pillars" -> RespawnPhase.SUMMONING_PILLARS;
            case "summoning_dragon" -> RespawnPhase.SUMMONING_DRAGON;
            case "end" -> RespawnPhase.END;
            default -> RespawnPhase.NONE;
        };
    }

    @Override
    public boolean setRespawnPhase(RespawnPhase phase) {
        return phase != RespawnPhase.NONE && getRespawnPhase() != RespawnPhase.NONE
            && LeftoverNatives.dragonAction(world.getName(), "stage", phase.ordinal());
    }

    @Override
    public void resetCrystals() {
        LeftoverNatives.dragonAction(world.getName(), "crystals", 0);
    }

    @Override
    public int getGatewayCount() {
        return Integer.parseInt(facts()[3]);
    }

    @Override
    public boolean spawnNewGateway() {
        return LeftoverNatives.dragonAction(world.getName(), "gateway", 0);
    }

    @Override
    public void spawnNewGateway(io.papermc.paper.math.Position position) {
        throw Unsupported.hit("DragonBattle#spawnNewGateway");
    }

    @Override
    public List<EnderCrystal> getRespawnCrystals() {
        return List.of();
    }

    @Override
    public List<EnderCrystal> getHealingCrystals() {
        return List.of();
    }
}
