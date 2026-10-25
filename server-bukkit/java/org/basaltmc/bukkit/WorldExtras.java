package org.basaltmc.bukkit;

import java.util.Collection;
import java.util.List;
import java.util.Locale;
import org.bukkit.Effect;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.Particle;
import org.bukkit.Sound;
import org.bukkit.SoundCategory;
import org.bukkit.Tag;
import org.bukkit.WorldBorder;
import org.bukkit.block.data.BlockData;
import org.bukkit.entity.Entity;
import org.bukkit.entity.FallingBlock;
import org.bukkit.entity.Player;
import org.bukkit.material.MaterialData;

public abstract class WorldExtras extends org.bukkit.craftbukkit.CraftWorld {
    private BasaltWorldBorder border;

    private static String soundKey(Sound sound) {
        return sound.key().asString();
    }

    private static String soundKey(String sound) {
        return sound.indexOf(':') < 0 ? "minecraft:" + sound : sound;
    }

    private static int itemIndex(Material material) {
        String id = material.getKey().toString();
        String[] ids = Registries.itemIds();
        for (int i = 0; i < ids.length; i++) {
            if (ids[i].equals(id)) {
                return i;
            }
        }
        return 0;
    }

    private static String category(SoundCategory category) {
        return category.name().toLowerCase(Locale.ROOT);
    }

    private void play(String sound, SoundCategory category, double x, double y, double z, float volume, float pitch,
        long seed) {
        WorldEnvNatives.sound(getName(), sound, category(category), x, y, z, volume, pitch, seed);
    }

    @Override
    public void playSound(Location location, Sound sound, SoundCategory category, float volume, float pitch) {
        playSound(location, sound, category, volume, pitch, java.util.concurrent.ThreadLocalRandom.current().nextLong());
    }

    @Override
    public void playSound(Location location, String sound, SoundCategory category, float volume, float pitch) {
        playSound(location, sound, category, volume, pitch, java.util.concurrent.ThreadLocalRandom.current().nextLong());
    }

    @Override
    public void playSound(Location location, Sound sound, SoundCategory category, float volume, float pitch,
        long seed) {
        play(soundKey(sound), category, location.getX(), location.getY(), location.getZ(), volume, pitch, seed);
    }

    @Override
    public void playSound(Location location, String sound, SoundCategory category, float volume, float pitch,
        long seed) {
        play(soundKey(sound), category, location.getX(), location.getY(), location.getZ(), volume, pitch, seed);
    }

    @Override
    public void playSound(Entity entity, Sound sound, SoundCategory category, float volume, float pitch) {
        playSound(entity.getLocation(), sound, category, volume, pitch);
    }

    @Override
    public void playSound(Entity entity, String sound, SoundCategory category, float volume, float pitch) {
        playSound(entity.getLocation(), sound, category, volume, pitch);
    }

    @Override
    public void playSound(Entity entity, Sound sound, SoundCategory category, float volume, float pitch, long seed) {
        playSound(entity.getLocation(), sound, category, volume, pitch, seed);
    }

    @Override
    public void playSound(Entity entity, String sound, SoundCategory category, float volume, float pitch, long seed) {
        playSound(entity.getLocation(), sound, category, volume, pitch, seed);
    }

    @Override
    public <T> void spawnParticle(Particle particle, List<Player> receivers, Player sender, double x, double y,
        double z, int count, double offsetX, double offsetY, double offsetZ, double speedX, double speedY,
        double speedZ, T data, boolean force, Particle.RandomizationType randomization) {
        ParticleData converted = ParticleData.of(particle, data);
        int mode = randomization == null ? 0 : randomization.ordinal();
        String world = getName();
        if (receivers == null) {
            WorldEnvNatives.particle(world, particle.getKey().asString(), x, y, z, count, offsetX, offsetY, offsetZ,
                speedX, speedY, speedZ, mode, force, converted.numbers, converted.item, null);
            return;
        }
        for (Player receiver : receivers) {
            WorldEnvNatives.particle(world, particle.getKey().asString(), x, y, z, count, offsetX, offsetY, offsetZ,
                speedX, speedY, speedZ, mode, force, converted.numbers, converted.item,
                receiver.getUniqueId().toString());
        }
    }

    @Override
    public <T> void playEffect(Location location, Effect effect, T data, int radius) {
        if (data != null && !effect.isApplicable(data)) {
            throw new IllegalArgumentException("Effect " + effect + " does not take " + data.getClass());
        }
        int value = 0;
        if (data instanceof BlockData block) {
            value = BasaltBlockData.stateOf(block);
        } else if (data instanceof Material material) {
            value = material.isBlock() ? Registries.defaultStateOf(material) : itemIndex(material);
        } else if (data instanceof Integer number) {
            value = number;
        } else if (data instanceof org.bukkit.Color color) {
            value = color.asRGB();
        }
        playEffect(location, effect, value, radius);
    }

    @Override
    public void playEffect(Location location, Effect effect, int data, int radius) {
        boolean global = radius <= 0 || radius > 64;
        WorldEnvNatives.levelEvent(getName(), effect.getId(), location.getBlockX(), location.getBlockY(),
            location.getBlockZ(), data, global);
    }

    @Override
    public WorldBorder getWorldBorder() {
        if (border == null) {
            border = new BasaltWorldBorder((BasaltWorld) this);
        }
        return border;
    }

    @Override
    public FallingBlock spawnFallingBlock(Location location, MaterialData data) throws IllegalArgumentException {
        return spawnFallingBlock(location, data.getItemType().createBlockData());
    }

    @Override
    public FallingBlock spawnFallingBlock(Location location, Material material, byte data)
        throws IllegalArgumentException {
        return spawnFallingBlock(location, material.createBlockData());
    }

    @Override
    public boolean isUltraWarm() {
        return getEnvironment() == Environment.NETHER;
    }

    @Override
    public boolean isPiglinSafe() {
        return getEnvironment() == Environment.NETHER;
    }

    @Override
    public boolean isRespawnAnchorWorks() {
        return getEnvironment() == Environment.NETHER;
    }

    @Override
    public boolean isBedWorks() {
        return getEnvironment() == Environment.NORMAL;
    }

    @Override
    public boolean hasRaids() {
        return getEnvironment() == Environment.NORMAL;
    }

    @Override
    public boolean hasBonusChest() {
        return false;
    }

    @Override
    public boolean isFixedTime() {
        return getEnvironment() == Environment.NETHER;
    }

    @Override
    public boolean isClearWeather() {
        return !hasStorm() && !isThundering();
    }

    @Override
    public io.papermc.paper.world.MoonPhase getMoonPhase() {
        return io.papermc.paper.world.MoonPhase.getPhase(getFullTime());
    }

    @Override
    public Collection<Material> getInfiniburn() {
        String name = switch (getEnvironment()) {
            case NETHER -> "infiniburn_nether";
            case THE_END -> "infiniburn_end";
            default -> "infiniburn_overworld";
        };
        Tag<Material> tag = org.bukkit.Bukkit.getTag(Tag.REGISTRY_BLOCKS, NamespacedKey.minecraft(name),
            Material.class);
        return tag == null ? List.of() : tag.getValues();
    }

    @Override
    public int getPlayerCount() {
        return WorldEnvNatives.playerCount(getName());
    }

    @Override
    public int getTileEntityCount() {
        return WorldEnvNatives.blockEntityCount(getName());
    }

    @Override
    public int getChunkCount() {
        int[] loaded = WorldNatives.loadedChunks(getName());
        return loaded == null ? 0 : loaded.length / 2;
    }

    @Override
    public void sendPluginMessage(org.bukkit.plugin.Plugin source, String channel, byte[] message) {
        for (Player player : getPlayers()) {
            player.sendPluginMessage(source, channel, message);
        }
    }

    @Override
    public java.util.Set<String> getListeningPluginChannels() {
        java.util.Set<String> channels = new java.util.HashSet<>();
        for (Player player : getPlayers()) {
            channels.addAll(player.getListeningPluginChannels());
        }
        return channels;
    }

    @Override
    public org.bukkit.World.Spigot spigot() {
        return new org.bukkit.World.Spigot() {
            @Override
            public org.bukkit.entity.LightningStrike strikeLightning(Location location, boolean isSilent) {
                return WorldExtras.this.strikeLightning(location);
            }

            @Override
            public org.bukkit.entity.LightningStrike strikeLightningEffect(Location location, boolean isSilent) {
                return WorldExtras.this.strikeLightningEffect(location);
            }
        };
    }
}
