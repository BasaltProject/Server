package org.basaltmc.bukkit.misc;

import io.papermc.paper.advancement.AdvancementDisplay;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.advancement.Advancement;
import org.bukkit.advancement.AdvancementRequirement;
import org.bukkit.advancement.AdvancementRequirements;
import org.bukkit.inventory.ItemStack;

public final class BasaltAdvancement implements Advancement {
    private final String id;

    public BasaltAdvancement(String id) {
        this.id = id;
    }

    public static Advancement get(NamespacedKey key) {
        return MiscNatives.advInfo(key.toString()) == null ? null : new BasaltAdvancement(key.toString());
    }

    public static List<Advancement> all() {
        List<Advancement> out = new ArrayList<>();
        for (String id : MiscNatives.advIds()) {
            out.add(new BasaltAdvancement(id));
        }
        return out;
    }

    private String[] info() {
        String[] info = MiscNatives.advInfo(id);
        if (info == null) {
            throw new IllegalStateException("The advancement " + id + " is gone");
        }
        return info;
    }

    @Override
    public NamespacedKey getKey() {
        return NamespacedKey.fromString(id);
    }

    @Override
    public Collection<String> getCriteria() {
        return List.of(MiscNatives.advLists(id, 0));
    }

    @Override
    public AdvancementRequirements getRequirements() {
        List<AdvancementRequirement> out = new ArrayList<>();
        for (String group : MiscNatives.advLists(id, 2)) {
            List<String> names = List.of(group.split("\n"));
            out.add(new AdvancementRequirement() {
                @Override
                public List<String> getRequiredCriteria() {
                    return names;
                }

                @Override
                public boolean isStrict() {
                    return names.size() == 1;
                }
            });
        }
        return () -> out;
    }

    @Override
    public AdvancementDisplay getDisplay() {
        String[] info = info();
        if (!"1".equals(info[3])) {
            return null;
        }
        Component title = Json.component(info[4]);
        Component description = Json.component(info[5]);
        AdvancementDisplay.Frame frame = AdvancementDisplay.Frame.NAMES.valueOrThrow(info[7]);
        Material icon = Material.matchMaterial(info[6]);
        NamespacedKey background = info[8] == null ? null : NamespacedKey.fromString(info[8]);
        return new AdvancementDisplay() {
            @Override
            public Frame frame() {
                return frame;
            }

            @Override
            public Component title() {
                return title;
            }

            @Override
            public Component description() {
                return description;
            }

            @Override
            public ItemStack icon() {
                return new ItemStack(icon == null ? Material.AIR : icon);
            }

            @Override
            public boolean doesShowToast() {
                return "1".equals(info[9]);
            }

            @Override
            public boolean doesAnnounceToChat() {
                return "1".equals(info[10]);
            }

            @Override
            public boolean isHidden() {
                return "1".equals(info[11]);
            }

            @Override
            public NamespacedKey backgroundPath() {
                return background;
            }

            @Override
            public Component displayName() {
                return Component.translatable("chat.square_brackets",
                    title.hoverEvent(net.kyori.adventure.text.event.HoverEvent.showText(
                        Component.text().append(title).appendNewline().append(description).build())))
                    .color(frame.color());
            }
        };
    }

    @Override
    public Component displayName() {
        AdvancementDisplay display = getDisplay();
        return display != null ? display.displayName() : Component.text(id);
    }

    @Override
    public Advancement getParent() {
        String parent = info()[1];
        return parent == null ? null : new BasaltAdvancement(parent);
    }

    @Override
    public Collection<Advancement> getChildren() {
        List<Advancement> out = new ArrayList<>();
        for (String child : MiscNatives.advLists(id, 1)) {
            out.add(new BasaltAdvancement(child));
        }
        return out;
    }

    @Override
    public Advancement getRoot() {
        return new BasaltAdvancement(info()[2]);
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltAdvancement advancement && advancement.id.equals(id);
    }

    @Override
    public int hashCode() {
        return id.hashCode();
    }
}
