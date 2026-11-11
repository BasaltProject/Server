// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.core.Holder;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.StringRepresentable;
public enum NoteBlockInstrument implements StringRepresentable {
    HARP, BASEDRUM, SNARE, HAT, BASS, FLUTE, BELL, GUITAR, CHIME, XYLOPHONE, IRON_XYLOPHONE, COW_BELL, DIDGERIDOO, BIT, BANJO, PLING, TRUMPET, TRUMPET_EXPOSED, TRUMPET_OXIDIZED, TRUMPET_WEATHERED, ZOMBIE, SKELETON, CREEPER, DRAGON, WITHER_SKELETON, PIGLIN, CUSTOM_HEAD;
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
    public Holder<SoundEvent> getSoundEvent() {
        return switch (this) {
            case HARP -> SoundEvents.NOTE_BLOCK_HARP;
            case BASEDRUM -> SoundEvents.NOTE_BLOCK_BASEDRUM;
            case SNARE -> SoundEvents.NOTE_BLOCK_SNARE;
            case HAT -> SoundEvents.NOTE_BLOCK_HAT;
            case BASS -> SoundEvents.NOTE_BLOCK_BASS;
            case FLUTE -> SoundEvents.NOTE_BLOCK_FLUTE;
            case BELL -> SoundEvents.NOTE_BLOCK_BELL;
            case GUITAR -> SoundEvents.NOTE_BLOCK_GUITAR;
            case CHIME -> SoundEvents.NOTE_BLOCK_CHIME;
            case XYLOPHONE -> SoundEvents.NOTE_BLOCK_XYLOPHONE;
            case IRON_XYLOPHONE -> SoundEvents.NOTE_BLOCK_IRON_XYLOPHONE;
            case COW_BELL -> SoundEvents.NOTE_BLOCK_COW_BELL;
            case DIDGERIDOO -> SoundEvents.NOTE_BLOCK_DIDGERIDOO;
            case BIT -> SoundEvents.NOTE_BLOCK_BIT;
            case BANJO -> SoundEvents.NOTE_BLOCK_BANJO;
            case PLING -> SoundEvents.NOTE_BLOCK_PLING;
            case TRUMPET -> SoundEvents.NOTE_BLOCK_TRUMPET;
            case TRUMPET_EXPOSED -> SoundEvents.NOTE_BLOCK_TRUMPET_EXPOSED;
            case TRUMPET_OXIDIZED -> SoundEvents.NOTE_BLOCK_TRUMPET_OXIDIZED;
            case TRUMPET_WEATHERED -> SoundEvents.NOTE_BLOCK_TRUMPET_WEATHERED;
            case ZOMBIE -> SoundEvents.NOTE_BLOCK_IMITATE_ZOMBIE;
            case SKELETON -> SoundEvents.NOTE_BLOCK_IMITATE_SKELETON;
            case CREEPER -> SoundEvents.NOTE_BLOCK_IMITATE_CREEPER;
            case DRAGON -> SoundEvents.NOTE_BLOCK_IMITATE_ENDER_DRAGON;
            case WITHER_SKELETON -> SoundEvents.NOTE_BLOCK_IMITATE_WITHER_SKELETON;
            case PIGLIN -> SoundEvents.NOTE_BLOCK_IMITATE_PIGLIN;
            case CUSTOM_HEAD -> SoundEvents.UI_BUTTON_CLICK;
        };
    }
    public boolean isTunable() {
        return ordinal() <= TRUMPET_WEATHERED.ordinal();
    }
    public boolean hasCustomSound() {
        return this == CUSTOM_HEAD;
    }
    public boolean worksAboveNoteBlock() {
        return ordinal() > TRUMPET_WEATHERED.ordinal();
    }
    static enum Type {
        BASE_BLOCK, MOB_HEAD, CUSTOM;
    }
}
