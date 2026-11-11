// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr.event;
import jdk.jfr.Category;
import jdk.jfr.Enabled;
import jdk.jfr.Event;
import jdk.jfr.EventType;
import jdk.jfr.Label;
import jdk.jfr.Name;
import jdk.jfr.Period;
import jdk.jfr.StackTrace;
@Name("minecraft.ClientFps")
@Label("Client FPS")
@Category({"Minecraft", "Ticking"})
@StackTrace(false)
@Enabled(false)
@Period("1 s")
public class ClientFpsEvent extends Event {
    public static final String EVENT_NAME = "minecraft.ClientFps";
    public static final EventType TYPE = EventType.getEventType(ClientFpsEvent.class);
    @Name(Fields.FPS)
    @Label("Frames per second")
    public final int fps;
    public ClientFpsEvent(int fps) {
        this.fps = fps;
    }
    public static class Fields {
        public static final String FPS = "fps";
        private Fields() {
        }
    }
}
