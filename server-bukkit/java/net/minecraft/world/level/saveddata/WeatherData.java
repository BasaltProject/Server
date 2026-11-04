// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.saveddata;
import com.mojang.serialization.Codec;
public final class WeatherData extends SavedData {
    public static final Codec<WeatherData> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.saveddata.WeatherData.class, "CODEC");
    public static final SavedDataType<WeatherData> TYPE = null;
    private org.bukkit.World world;
    private int clearTime;
    private int rainTime;
    private int thunderTime;
    private boolean raining;
    private boolean thundering;

    public WeatherData() {
    }

    /** The weather of a world, read and written through the Bukkit view of it. */
    public WeatherData(org.bukkit.World world) {
        this.world = world;
    }
    public WeatherData(int clearWeatherTime, int rainTime, int thunderTime, boolean raining, boolean thundering) {
        this.clearTime = clearWeatherTime;
        this.rainTime = rainTime;
        this.thunderTime = thunderTime;
        this.raining = raining;
        this.thundering = thundering;
    }
    public int getClearWeatherTime() {
        return world == null ? clearTime : world.getClearWeatherDuration();
    }
    public void setClearWeatherTime(int clearWeatherTime) {
        if (world == null) {
            clearTime = clearWeatherTime;
        } else {
            world.setClearWeatherDuration(clearWeatherTime);
        }
    }
    public boolean isThundering() {
        return world == null ? thundering : world.isThundering();
    }
    public void setThundering(boolean thundering) {
        if (world == null) {
            this.thundering = thundering;
        } else {
            world.setThundering(thundering);
        }
    }
    public int getThunderTime() {
        return world == null ? thunderTime : world.getThunderDuration();
    }
    public void setThunderTime(int thunderTime) {
        if (world == null) {
            this.thunderTime = thunderTime;
        } else {
            world.setThunderDuration(thunderTime);
        }
    }
    public boolean isRaining() {
        return world == null ? raining : world.hasStorm();
    }
    public void setRaining(boolean raining) {
        if (world == null) {
            this.raining = raining;
        } else {
            world.setStorm(raining);
        }
    }
    public int getRainTime() {
        return world == null ? rainTime : world.getWeatherDuration();
    }
    public void setRainTime(int rainTime) {
        if (world == null) {
            this.rainTime = rainTime;
        } else {
            world.setWeatherDuration(rainTime);
        }
    }
}
