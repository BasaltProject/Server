// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.gui;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Graphics;
import javax.swing.JComponent;
import javax.swing.Timer;
import net.minecraft.server.MinecraftServer;
public class StatsComponent extends JComponent {
    private static final int kSamples = 256;
    private static final int kBarHeight = 100;
    private static final int kTimerMs = 500;
    private final int[] values = new int[kSamples];
    private int head;
    private final String[] lines = new String[2];
    private final MinecraftServer server;
    private final Timer timer;
    public StatsComponent(MinecraftServer server) {
        this.server = server;
        setPreferredSize(new Dimension(456, 246));
        setMinimumSize(new Dimension(456, 246));
        setMaximumSize(new Dimension(456, 246));
        timer = new Timer(kTimerMs, event -> sample());
        timer.start();
        setBackground(Color.BLACK);
    }
    private void sample() {
        Runtime runtime = Runtime.getRuntime();
        long used = runtime.totalMemory() - runtime.freeMemory();
        lines[0] = "Memory use: " + used / 1024L / 1024L + " mb (" + runtime.freeMemory() * 100L / runtime.maxMemory() + "% free)";
        lines[1] = "Avg tick: " + String.format(java.util.Locale.ROOT, "%.1f", org.bukkit.Bukkit.getAverageTickTime()) + " ms";
        values[head++ & (kSamples - 1)] = (int) (used * kBarHeight / runtime.maxMemory());
        repaint();
    }
    public void paint(Graphics g) {
        g.setColor(new Color(0xFFFFFF));
        g.fillRect(0, 0, 456, 246);
        for (int index = 0; index < kSamples; index++) {
            int value = values[index + head & (kSamples - 1)];
            g.setColor(new Color(value + 28 << 16));
            g.fillRect(index, kBarHeight - value, 1, value);
        }
        g.setColor(Color.BLACK);
        int y = 20;
        for (String line : lines) {
            if (line != null) {
                g.drawString(line, 32, y);
                y += 16;
            }
        }
    }
    public void close() {
        timer.stop();
    }
}
