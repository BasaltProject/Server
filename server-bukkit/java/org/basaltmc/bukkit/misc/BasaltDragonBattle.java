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
        return new FightBar(world);
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
        org.basaltmc.bukkit.RestNatives.placeEndGateway(world.getName(), position.blockX(), position.blockY(),
            position.blockZ());
    }

    private List<EnderCrystal> crystals(String csv) {
        List<EnderCrystal> out = new java.util.ArrayList<>();
        for (String id : csv.isEmpty() ? new String[0] : csv.split(",")) {
            Entity found = Bukkit.getEntity(java.util.UUID.fromString(id));
            if (found instanceof EnderCrystal crystal && found.isValid()) {
                out.add(crystal);
            }
        }
        return List.copyOf(out);
    }

    @Override
    public List<EnderCrystal> getRespawnCrystals() {
        return crystals(facts()[8]);
    }

    @Override
    public List<EnderCrystal> getHealingCrystals() {
        return crystals(facts()[9]);
    }

    /** The bar of the fight: its progress and who sees it can change; its look is the game's own. */
    private static final class FightBar implements BossBar {
        private final World world;

        FightBar(World world) {
            this.world = world;
        }

        private String[] facts() {
            String[] facts = MiscNatives.dragonFight(world.getName());
            if (facts == null) {
                throw new IllegalStateException("The world has no dragon fight");
            }
            return facts;
        }

        @Override
        public String getTitle() {
            return "Ender Dragon";
        }

        @Override
        public void setTitle(String title) {
            throw Unsupported.hit("BossBar#setTitle on the dragon fight bar");
        }

        @Override
        public org.bukkit.boss.BarColor getColor() {
            return org.bukkit.boss.BarColor.PINK;
        }

        @Override
        public void setColor(org.bukkit.boss.BarColor color) {
            throw Unsupported.hit("BossBar#setColor on the dragon fight bar");
        }

        @Override
        public org.bukkit.boss.BarStyle getStyle() {
            return org.bukkit.boss.BarStyle.SOLID;
        }

        @Override
        public void setStyle(org.bukkit.boss.BarStyle style) {
            throw Unsupported.hit("BossBar#setStyle on the dragon fight bar");
        }

        @Override
        public void removeFlag(org.bukkit.boss.BarFlag flag) {
            throw Unsupported.hit("BossBar#removeFlag on the dragon fight bar");
        }

        @Override
        public void addFlag(org.bukkit.boss.BarFlag flag) {
            throw Unsupported.hit("BossBar#addFlag on the dragon fight bar");
        }

        @Override
        public boolean hasFlag(org.bukkit.boss.BarFlag flag) {
            return flag == org.bukkit.boss.BarFlag.PLAY_BOSS_MUSIC || flag == org.bukkit.boss.BarFlag.CREATE_FOG;
        }

        @Override
        public void setProgress(double progress) {
            if (progress < 0.0 || progress > 1.0) {
                throw new IllegalArgumentException("Progress must be between 0.0 and 1.0 (" + progress + ")");
            }
            LeftoverNatives.dragonAction(world.getName(), "bar_progress", (int) Math.round(progress * 10000.0));
        }

        @Override
        public double getProgress() {
            return Double.parseDouble(facts()[5]);
        }

        @Override
        public void addPlayer(org.bukkit.entity.Player player) {
            throw Unsupported.hit("BossBar#addPlayer on the dragon fight bar");
        }

        @Override
        public void removePlayer(org.bukkit.entity.Player player) {
            throw Unsupported.hit("BossBar#removePlayer on the dragon fight bar");
        }

        @Override
        public void removeAll() {
            throw Unsupported.hit("BossBar#removeAll on the dragon fight bar");
        }

        @Override
        public List<org.bukkit.entity.Player> getPlayers() {
            String csv = facts()[7];
            List<org.bukkit.entity.Player> out = new java.util.ArrayList<>();
            for (String id : csv.isEmpty() ? new String[0] : csv.split(",")) {
                int entityId = Integer.parseInt(id);
                for (org.bukkit.entity.Player player : world.getPlayers()) {
                    if (player.getEntityId() == entityId) {
                        out.add(player);
                    }
                }
            }
            return out;
        }

        @Override
        public void setVisible(boolean visible) {
            LeftoverNatives.dragonAction(world.getName(), "bar_visible", visible ? 1 : 0);
        }

        @Override
        public boolean isVisible() {
            return "1".equals(facts()[6]);
        }

        @Override
        public void show() {
            setVisible(true);
        }

        @Override
        public void hide() {
            setVisible(false);
        }
    }
}
