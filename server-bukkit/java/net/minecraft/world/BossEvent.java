// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;

import com.mojang.serialization.Codec;
import java.util.UUID;
import net.minecraft.ChatFormatting;
import net.minecraft.network.chat.Component;
import net.minecraft.util.StringRepresentable;

public abstract class BossEvent {
    private final UUID id;
    protected Component name;
    protected float progress;
    protected BossBarColor color;
    protected BossBarOverlay overlay;
    protected boolean darkenScreen;
    protected boolean playBossMusic;
    protected boolean createWorldFog;

    public BossEvent(UUID id, Component name, BossBarColor color, BossBarOverlay overlay) {
        this.id = id;
        this.name = name;
        this.color = color;
        this.overlay = overlay;
        this.progress = 1.0F;
    }

    public UUID getId() {
        return id;
    }

    public Component getName() {
        return name;
    }

    public void setName(Component name) {
        this.name = name;
    }

    public float getProgress() {
        return progress;
    }

    public void setProgress(float progress) {
        this.progress = progress;
    }

    public BossBarColor getColor() {
        return color;
    }

    public void setColor(BossBarColor color) {
        this.color = color;
    }

    public BossBarOverlay getOverlay() {
        return overlay;
    }

    public void setOverlay(BossBarOverlay overlay) {
        this.overlay = overlay;
    }

    public boolean shouldDarkenScreen() {
        return darkenScreen;
    }

    public BossEvent setDarkenScreen(boolean darkenScreen) {
        this.darkenScreen = darkenScreen;
        return this;
    }

    public boolean shouldPlayBossMusic() {
        return playBossMusic;
    }

    public BossEvent setPlayBossMusic(boolean playBossMusic) {
        this.playBossMusic = playBossMusic;
        return this;
    }

    public BossEvent setCreateWorldFog(boolean createWorldFog) {
        this.createWorldFog = createWorldFog;
        return this;
    }

    public boolean shouldCreateWorldFog() {
        return createWorldFog;
    }

    public static enum BossBarColor implements StringRepresentable {
        PINK("pink", ChatFormatting.RED), BLUE("blue", ChatFormatting.BLUE), RED("red", ChatFormatting.DARK_RED),
        GREEN("green", ChatFormatting.GREEN), YELLOW("yellow", ChatFormatting.YELLOW), PURPLE("purple", ChatFormatting.DARK_BLUE),
        WHITE("white", ChatFormatting.WHITE);

        public static final Codec<BossBarColor> CODEC = StringRepresentable.fromEnum(BossBarColor::values);
        private final String name;
        private final ChatFormatting formatting;

        BossBarColor(String name, ChatFormatting formatting) {
            this.name = name;
            this.formatting = formatting;
        }

        public ChatFormatting getFormatting() {
            return formatting;
        }

        public String getName() {
            return name;
        }

        public String getSerializedName() {
            return name;
        }
    }

    public static enum BossBarOverlay implements StringRepresentable {
        PROGRESS("progress"), NOTCHED_6("notched_6"), NOTCHED_10("notched_10"), NOTCHED_12("notched_12"), NOTCHED_20("notched_20");

        public static final Codec<BossBarOverlay> CODEC = StringRepresentable.fromEnum(BossBarOverlay::values);
        private final String name;

        BossBarOverlay(String name) {
            this.name = name;
        }

        public String getName() {
            return name;
        }

        public String getSerializedName() {
            return name;
        }
    }
}
