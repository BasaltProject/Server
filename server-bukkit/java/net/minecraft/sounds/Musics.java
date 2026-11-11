// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.sounds;
import net.minecraft.core.Holder;
public class Musics {
    private static final int kSecond = 20;
    private static final int kHalfMinute = 600;
    private static final int kFiveMinutes = 6000;
    private static final int kTenMinutes = 12000;
    private static final int kTwentyMinutes = 24000;
    public static final Music MENU = new Music(SoundEvents.MUSIC_MENU, kSecond, kHalfMinute, true);
    public static final Music CREATIVE = new Music(SoundEvents.MUSIC_CREATIVE, kTenMinutes, kTwentyMinutes, false);
    public static final Music CREDITS = new Music(SoundEvents.MUSIC_CREDITS, 0, 0, true);
    public static final Music END_BOSS = new Music(SoundEvents.MUSIC_DRAGON, 0, 0, true);
    public static final Music END = new Music(SoundEvents.MUSIC_END, kFiveMinutes, kTwentyMinutes, true);
    public static final Music UNDER_WATER = createGameMusic(SoundEvents.MUSIC_UNDER_WATER);
    public static final Music GAME = createGameMusic(SoundEvents.MUSIC_GAME);
    public static Music createGameMusic(Holder<SoundEvent> soundEvent) {
        return new Music(soundEvent, kTenMinutes, kTwentyMinutes, false);
    }
}
