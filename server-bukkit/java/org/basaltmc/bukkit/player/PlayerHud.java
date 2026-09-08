package org.basaltmc.bukkit.player;

import java.util.UUID;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.title.TitlePart;
import net.md_5.bungee.api.chat.BaseComponent;
import org.basaltmc.bukkit.BasaltBlockData;
import org.basaltmc.bukkit.Text;
import org.basaltmc.bukkit.gen.PlayerBase;
import org.bukkit.Effect;
import org.bukkit.Instrument;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.Note;
import org.bukkit.Particle;
import org.bukkit.SoundCategory;
import org.bukkit.entity.Entity;

public abstract class PlayerHud extends PlayerBase {
    private static final int kDefaultFadeIn = 10;
    private static final int kDefaultStay = 70;
    private static final int kDefaultFadeOut = 20;

    private volatile Component displayName;

    private UUID key() {
        return getUniqueId();
    }

    @Override
    public Component displayName() {
        Component held = displayName;
        return held != null ? held : Component.text(getName());
    }

    @Override
    public void displayName(Component name) {
        displayName = name;
    }

    @Override
    public String getDisplayName() {
        return Text.legacy(displayName());
    }

    @Override
    public void setDisplayName(String name) {
        displayName = name == null ? null : net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer
            .legacySection().deserialize(name);
    }

    @Override
    public String getPlayerListName() {
        return Text.legacy(playerListName());
    }

    @Override
    public Component teamDisplayName() {
        return displayName();
    }

    private static String mini(Component component) {
        return component == null ? "" : Text.mini(component);
    }

    @Override
    public void sendMessage(Component message, net.kyori.adventure.chat.ChatType.Bound boundChatType) {
        sendMessage(PlayerChat.decorate(message, boundChatType));
    }

    @Override
    public void sendMessage(net.kyori.adventure.chat.SignedMessage signedMessage,
            net.kyori.adventure.chat.ChatType.Bound boundChatType) {
        Component content = signedMessage.unsignedContent() != null ? signedMessage.unsignedContent()
            : Component.text(signedMessage.message());
        sendMessage(PlayerChat.decorate(content, boundChatType));
    }

    @Override
    public void sendPlayerListHeader(Component header) {
        PlayerCalls.set(key(), "tab_header", header == null ? null : mini(header));
    }

    @Override
    public void sendPlayerListFooter(Component footer) {
        PlayerCalls.set(key(), "tab_footer", footer == null ? null : mini(footer));
    }

    @Override
    public void sendPlayerListHeaderAndFooter(Component header, Component footer) {
        PlayerCalls.call(key(), "tab_list", header == null ? null : mini(header), footer == null ? null : mini(footer));
    }

    @Override
    public Component playerListHeader() {
        String text = PlayerCalls.text(key(), "tab_header");
        return text == null ? null : net.kyori.adventure.text.minimessage.MiniMessage.miniMessage().deserialize(text);
    }

    @Override
    public Component playerListFooter() {
        String text = PlayerCalls.text(key(), "tab_footer");
        return text == null ? null : net.kyori.adventure.text.minimessage.MiniMessage.miniMessage().deserialize(text);
    }

    @Override
    public String getPlayerListHeader() {
        Component header = playerListHeader();
        return header == null ? null : Text.legacy(header);
    }

    @Override
    public String getPlayerListFooter() {
        Component footer = playerListFooter();
        return footer == null ? null : Text.legacy(footer);
    }

    @Override
    public void setPlayerListHeader(String header) {
        PlayerCalls.set(key(), "tab_header", header == null ? null : Text.mini(header));
    }

    @Override
    public void setPlayerListFooter(String footer) {
        PlayerCalls.set(key(), "tab_footer", footer == null ? null : Text.mini(footer));
    }

    @Override
    public void setPlayerListHeaderFooter(String header, String footer) {
        PlayerCalls.call(key(), "tab_list", header == null ? null : Text.mini(header),
            footer == null ? null : Text.mini(footer));
    }

    @Override
    public void setPlayerListHeaderFooter(BaseComponent header, BaseComponent footer) {
        setPlayerListHeaderFooter(header == null ? null : new BaseComponent[] {header},
            footer == null ? null : new BaseComponent[] {footer});
    }

    @Override
    public void setPlayerListHeaderFooter(BaseComponent[] header, BaseComponent[] footer) {
        PlayerCalls.call(key(), "tab_list", header == null ? null : mini(PlayerChat.component(header)),
            footer == null ? null : mini(PlayerChat.component(footer)));
    }

    @Override
    public void setPlayerListName(String name) {
        PlayerCalls.set(key(), "list_name", name == null ? null : Text.mini(name));
    }

    @Override
    public void playerListName(Component name) {
        PlayerCalls.set(key(), "list_name", name == null ? null : mini(name));
    }

    @Override
    public Component playerListName() {
        String text = PlayerCalls.text(key(), "list_name");
        return text == null ? Component.text(getName())
            : net.kyori.adventure.text.minimessage.MiniMessage.miniMessage().deserialize(text);
    }

    @Override
    public void setPlayerListOrder(int order) {
        PlayerCalls.set(key(), "list_order", (long) order);
    }

    @Override
    public int getPlayerListOrder() {
        return (int) PlayerCalls.integer(key(), "list_order");
    }

    @Override
    public void sendActionBar(char alternateChar, String message) {
        sendActionBar(net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer.legacy(alternateChar)
            .deserialize(message == null ? "" : message));
    }

    @Override
    public <T> void sendTitlePart(TitlePart<T> part, T value) {
        if (part == TitlePart.TITLE) {
            PlayerCalls.call(key(), "title_part", "title", mini((Component) value));
        } else if (part == TitlePart.SUBTITLE) {
            PlayerCalls.call(key(), "title_part", "subtitle", mini((Component) value));
        } else if (part == TitlePart.TIMES) {
            net.kyori.adventure.title.Title.Times times = (net.kyori.adventure.title.Title.Times) value;
            PlayerCalls.call(key(), "title_times", ticks(times.fadeIn()), ticks(times.stay()), ticks(times.fadeOut()));
        }
    }

    private static long ticks(java.time.Duration duration) {
        return duration.toMillis() / 50;
    }

    @Override
    public void clearTitle() {
        PlayerCalls.call(key(), "clear_title", false);
    }

    @Override
    public void resetTitle() {
        PlayerCalls.call(key(), "clear_title", true);
    }

    @Override
    public void hideTitle() {
        PlayerCalls.call(key(), "clear_title", false);
    }

    @Override
    public void setTitleTimes(int fadeIn, int stay, int fadeOut) {
        PlayerCalls.call(key(), "title_times", (long) fadeIn, (long) stay, (long) fadeOut);
    }

    @Override
    public void setSubtitle(BaseComponent subtitle) {
        PlayerCalls.call(key(), "title_part", "subtitle", mini(PlayerChat.component(subtitle)));
    }

    @Override
    public void setSubtitle(BaseComponent[] subtitle) {
        PlayerCalls.call(key(), "title_part", "subtitle", mini(PlayerChat.component(subtitle)));
    }

    @Override
    public void showTitle(BaseComponent title, BaseComponent subtitle, int fadeIn, int stay, int fadeOut) {
        showTitle(new BaseComponent[] {title}, subtitle == null ? null : new BaseComponent[] {subtitle}, fadeIn, stay,
            fadeOut);
    }

    @Override
    public void showTitle(BaseComponent[] title, BaseComponent[] subtitle, int fadeIn, int stay, int fadeOut) {
        PlayerCalls.call(key(), "title_times", (long) fadeIn, (long) stay, (long) fadeOut);
        if (subtitle != null) {
            PlayerCalls.call(key(), "title_part", "subtitle", mini(PlayerChat.component(subtitle)));
        }
        PlayerCalls.call(key(), "title_part", "title", mini(PlayerChat.component(title)));
    }

    @Override
    public void sendTitle(com.destroystokyo.paper.Title title) {
        updateTitle(title);
    }

    @Override
    public void updateTitle(com.destroystokyo.paper.Title title) {
        PlayerCalls.call(key(), "title_times", (long) title.getFadeIn(), (long) title.getStay(),
            (long) title.getFadeOut());
        if (title.getSubtitle() != null) {
            PlayerCalls.call(key(), "title_part", "subtitle", mini(PlayerChat.component(title.getSubtitle())));
        }
        PlayerCalls.call(key(), "title_part", "title", mini(PlayerChat.component(title.getTitle())));
    }

    @Override
    public void showTitle(net.kyori.adventure.title.Title title) {
        net.kyori.adventure.title.Title.Times times = title.times();
        PlayerCalls.call(key(), "title_times",
            times == null ? (long) kDefaultFadeIn : ticks(times.fadeIn()),
            times == null ? (long) kDefaultStay : ticks(times.stay()),
            times == null ? (long) kDefaultFadeOut : ticks(times.fadeOut()));
        PlayerCalls.call(key(), "title_part", "subtitle", mini(title.subtitle()));
        PlayerCalls.call(key(), "title_part", "title", mini(title.title()));
    }

    @Override
    public void sendTitle(String title, String subtitle, int fadeIn, int stay, int fadeOut) {
        PlayerCalls.call(key(), "title_times", (long) fadeIn, (long) stay, (long) fadeOut);
        PlayerCalls.call(key(), "title_part", "subtitle", subtitle == null ? "" : Text.mini(subtitle));
        PlayerCalls.call(key(), "title_part", "title", title == null ? "" : Text.mini(title));
    }

    @Override
    public void sendTitle(String title, String subtitle) {
        sendTitle(title, subtitle, kDefaultFadeIn, kDefaultStay, kDefaultFadeOut);
    }

    private static String category(SoundCategory category) {
        String name = (category == null ? SoundCategory.MASTER : category).name().toLowerCase(java.util.Locale.ROOT);
        return name.equals("blocks") ? "block" : name;
    }

    private static String category(net.kyori.adventure.sound.Sound.Source source) {
        return source == null ? "master" : switch (source) {
            case MASTER -> "master";
            case MUSIC -> "music";
            case RECORD -> "record";
            case WEATHER -> "weather";
            case BLOCK -> "block";
            case HOSTILE -> "hostile";
            case NEUTRAL -> "neutral";
            case PLAYER -> "player";
            case AMBIENT -> "ambient";
            case VOICE -> "voice";
            case UI -> "ui";
        };
    }

    private void soundAt(String sound, String category, double x, double y, double z, float volume, float pitch,
            long seed) {
        PlayerCalls.call(key(), "sound_at", sound, category, x, y, z, (double) volume, (double) pitch, seed);
    }

    private void soundOn(Entity entity, String sound, String category, float volume, float pitch, long seed) {
        PlayerCalls.call(key(), "sound_entity", sound, category, (long) entity.getEntityId(), (double) volume,
            (double) pitch, seed);
    }

    private static String name(org.bukkit.Sound sound) {
        return sound.getKey().toString();
    }

    @Override
    public void playSound(Location location, String sound, float volume, float pitch) {
        playSound(location, sound, SoundCategory.MASTER, volume, pitch, randomSeed());
    }

    @Override
    public void playSound(Location location, String sound, SoundCategory category, float volume, float pitch) {
        playSound(location, sound, category, volume, pitch, randomSeed());
    }

    @Override
    public void playSound(Location location, String sound, SoundCategory category, float volume, float pitch,
            long seed) {
        soundAt(sound, category(category), location.getX(), location.getY(), location.getZ(), volume, pitch, seed);
    }

    @Override
    public void playSound(Location location, org.bukkit.Sound sound, SoundCategory category, float volume,
            float pitch) {
        playSound(location, name(sound), category, volume, pitch, randomSeed());
    }

    @Override
    public void playSound(Location location, org.bukkit.Sound sound, SoundCategory category, float volume,
            float pitch, long seed) {
        playSound(location, name(sound), category, volume, pitch, seed);
    }

    @Override
    public void playSound(Entity entity, String sound, SoundCategory category, float volume, float pitch) {
        soundOn(entity, sound, category(category), volume, pitch, randomSeed());
    }

    @Override
    public void playSound(Entity entity, String sound, SoundCategory category, float volume, float pitch,
            long seed) {
        soundOn(entity, sound, category(category), volume, pitch, seed);
    }

    @Override
    public void playSound(Entity entity, org.bukkit.Sound sound, SoundCategory category, float volume, float pitch) {
        soundOn(entity, name(sound), category(category), volume, pitch, randomSeed());
    }

    @Override
    public void playSound(Entity entity, org.bukkit.Sound sound, SoundCategory category, float volume, float pitch,
            long seed) {
        soundOn(entity, name(sound), category(category), volume, pitch, seed);
    }

    private static long randomSeed() {
        return java.util.concurrent.ThreadLocalRandom.current().nextLong();
    }

    @Override
    public void playSound(net.kyori.adventure.sound.Sound sound) {
        Location here = getLocation();
        playSound(sound, here.getX(), here.getY(), here.getZ());
    }

    @Override
    public void playSound(net.kyori.adventure.sound.Sound sound, double x, double y, double z) {
        soundAt(sound.name().asString(), category(sound.source()), x, y, z, sound.volume(), sound.pitch(),
            sound.seed().orElse(randomSeed()));
    }

    @Override
    public void playSound(net.kyori.adventure.sound.Sound sound, net.kyori.adventure.sound.Sound.Emitter emitter) {
        if (emitter == net.kyori.adventure.sound.Sound.Emitter.self()) {
            soundOn(this, sound.name().asString(), category(sound.source()), sound.volume(), sound.pitch(),
                sound.seed().orElse(randomSeed()));
        } else if (emitter instanceof Entity entity) {
            soundOn(entity, sound.name().asString(), category(sound.source()), sound.volume(), sound.pitch(),
                sound.seed().orElse(randomSeed()));
        }
    }

    @Override
    public void stopSound(net.kyori.adventure.sound.Sound sound) {
        PlayerCalls.call(key(), "stop_sound", sound.name().asString(), category(sound.source()));
    }

    @Override
    public void stopSound(net.kyori.adventure.sound.SoundStop stop) {
        PlayerCalls.call(key(), "stop_sound", stop.sound() == null ? null : stop.sound().asString(),
            stop.source() == null ? null : category(stop.source()));
    }

    @Override
    public void stopSound(String sound, SoundCategory category) {
        PlayerCalls.call(key(), "stop_sound", sound, category == null ? null : category(category));
    }

    @Override
    public void stopSound(SoundCategory category) {
        PlayerCalls.call(key(), "stop_sound", null, category(category));
    }

    @Override
    public void stopAllSounds() {
        PlayerCalls.call(key(), "stop_sound", null, null);
    }

    @Override
    public void playNote(Location location, Instrument instrument, Note note) {
        String sound = instrument.getSound().getKey().toString();
        float pitch = (float) Math.pow(2.0, (note.getId() - 12.0) / 12.0);
        soundAt(sound, "record", location.getX(), location.getY(), location.getZ(), 3.0F, pitch, 0L);
    }

    @Override
    public void playNote(Location location, byte instrument, byte note) {
        playNote(location, Instrument.getByType(instrument), new Note(note));
    }

    @Override
    public void playEffect(Location location, Effect effect, int data) {
        PlayerCalls.call(key(), "world_event", (long) effect.getId(), (long) location.getBlockX(),
            (long) location.getBlockY(), (long) location.getBlockZ(), (long) data);
    }

    @Override
    public <T> void playEffect(Location location, Effect effect, T data) {
        int value = 0;
        if (data instanceof Material material) {
            value = BasaltBlockData.stateOf(material.createBlockData());
        } else if (data instanceof org.bukkit.block.data.BlockData blockData) {
            value = BasaltBlockData.stateOf(blockData);
        } else if (data instanceof Integer number) {
            value = number;
        } else if (data instanceof org.bukkit.block.BlockFace face) {
            value = effectDirection(face);
        } else if (data != null) {
            throw new IllegalArgumentException("Wrong kind of data for the effect " + effect);
        }
        playEffect(location, effect, value);
    }

    private static int effectDirection(org.bukkit.block.BlockFace face) {
        return switch (face) {
            case SOUTH_EAST -> 0;
            case SOUTH -> 1;
            case SOUTH_WEST -> 2;
            case EAST -> 3;
            case WEST -> 5;
            case NORTH_EAST -> 6;
            case NORTH -> 7;
            case NORTH_WEST -> 8;
            default -> 4;
        };
    }

    @Override
    public void playEffect(org.bukkit.EntityEffect effect) {
        PlayerCalls.call(key(), "entity_event", (long) getEntityId(), (long) effect.getData());
    }

    @Override
    public void sendEntityEffect(org.bukkit.EntityEffect effect, Entity target) {
        PlayerCalls.call(key(), "entity_event", (long) target.getEntityId(), (long) effect.getData());
    }

    @Override
    public <T> void spawnParticle(Particle particle, double x, double y, double z, int count, double offsetX,
            double offsetY, double offsetZ, double speedX, double speedY, double speedZ, T data, boolean force,
            Particle.RandomizationType randomization) {
        Object[] extra = ParticleData.of(particle, data);
        Object[] arguments = new Object[13 + extra.length];
        arguments[0] = particle.getKey().toString();
        arguments[1] = x;
        arguments[2] = y;
        arguments[3] = z;
        arguments[4] = (long) count;
        arguments[5] = offsetX;
        arguments[6] = offsetY;
        arguments[7] = offsetZ;
        arguments[8] = speedX;
        arguments[9] = speedY;
        arguments[10] = speedZ;
        arguments[11] = force;
        arguments[12] = (long) (randomization == null ? 0 : randomization.ordinal());
        System.arraycopy(extra, 0, arguments, 13, extra.length);
        if (!PlayerCalls.done(key(), "particle_at", arguments)) {
            throw PlayerCalls.unsupported("Player#spawnParticle(" + particle.getKey() + ")");
        }
    }

    @Override
    public void updateCommands() {
        PlayerCalls.call(key(), "update_commands");
    }
}
