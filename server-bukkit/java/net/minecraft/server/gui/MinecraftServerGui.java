// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.gui;
import java.awt.BorderLayout;
import java.awt.Dimension;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.util.ArrayList;
import java.util.List;
import javax.swing.BorderFactory;
import javax.swing.JComponent;
import javax.swing.JFrame;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.SwingUtilities;
import javax.swing.Timer;
import net.minecraft.server.dedicated.DedicatedServer;
public class MinecraftServerGui extends JComponent {
    private static final String kTitle = "Minecraft server";
    private static final int kMaxLines = 2000;
    private final DedicatedServer server;
    private final List<Runnable> finalizers = new ArrayList<>();
    private final StatsComponent stats;
    private final PlayerListComponent players;
    private JTextArea console;
    private Timer timer;
    public static MinecraftServerGui showFrameFor(DedicatedServer server) {
        MinecraftServerGui gui = new MinecraftServerGui(server);
        JFrame frame = new JFrame(kTitle);
        frame.add(gui);
        frame.pack();
        frame.setLocationRelativeTo(null);
        frame.setVisible(true);
        frame.addWindowListener(new WindowAdapter() {
            public void windowClosing(WindowEvent event) {
                gui.close();
                frame.dispose();
            }
        });
        gui.addFinalizer(frame::dispose);
        return gui;
    }
    private MinecraftServerGui(DedicatedServer server) {
        this.server = server;
        this.stats = new StatsComponent(server);
        this.players = new PlayerListComponent(server);
        setPreferredSize(new Dimension(854, 480));
        setLayout(new BorderLayout());
        add(buildChatPanel(), BorderLayout.CENTER);
        add(buildInfoPanel(), BorderLayout.EAST);
    }
    private JComponent buildInfoPanel() {
        JPanel panel = new JPanel(new BorderLayout());
        panel.add(stats, BorderLayout.NORTH);
        panel.add(new JScrollPane(players), BorderLayout.CENTER);
        panel.setBorder(BorderFactory.createTitledBorder("Stats"));
        return panel;
    }
    private JComponent buildChatPanel() {
        JPanel panel = new JPanel(new BorderLayout());
        console = new JTextArea();
        console.setEditable(false);
        console.setFont(new java.awt.Font("Monospaced", java.awt.Font.PLAIN, 12));
        JScrollPane scroll = new JScrollPane(console, JScrollPane.VERTICAL_SCROLLBAR_ALWAYS, JScrollPane.HORIZONTAL_SCROLLBAR_NEVER);
        JTextField input = new JTextField();
        input.addActionListener(event -> {
            String command = input.getText().trim();
            if (!command.isEmpty()) {
                server.execute(() -> org.bukkit.Bukkit.dispatchCommand(org.bukkit.Bukkit.getConsoleSender(), command));
            }
            input.setText("");
        });
        panel.add(scroll, BorderLayout.CENTER);
        panel.add(input, BorderLayout.SOUTH);
        panel.setBorder(BorderFactory.createTitledBorder("Log and chat"));
        return panel;
    }
    public void addFinalizer(Runnable finalizer) {
        finalizers.add(finalizer);
    }
    public void start() {
        timer = new Timer(20, event -> players.tick());
        timer.start();
    }
    public void close() {
        if (timer != null) {
            timer.stop();
        }
        stats.close();
        for (Runnable finalizer : finalizers) {
            finalizer.run();
        }
    }
    public void print(JTextArea console, JScrollPane scrollPane, String line) {
        if (!SwingUtilities.isEventDispatchThread()) {
            SwingUtilities.invokeLater(() -> print(console, scrollPane, line));
            return;
        }
        var bar = scrollPane.getVerticalScrollBar();
        boolean atBottom = bar.getValue() + bar.getSize().getHeight() + 64 >= bar.getMaximum();
        console.append(line);
        int overflow = console.getLineCount() - kMaxLines;
        if (overflow > 0) {
            try {
                console.replaceRange("", 0, console.getLineEndOffset(overflow - 1));
            } catch (javax.swing.text.BadLocationException ignored) {
                // line vanished while trimming
            }
        }
        if (atBottom) {
            bar.setValue(Integer.MAX_VALUE);
        }
    }
}
