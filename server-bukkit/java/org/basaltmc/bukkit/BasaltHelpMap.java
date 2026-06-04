package org.basaltmc.bukkit;

import java.util.Collection;
import java.util.List;
import org.bukkit.help.HelpTopic;
import org.bukkit.help.HelpTopicFactory;
import org.basaltmc.bukkit.gen.HelpMapBase;

/** There is no Bukkit help index: Basalt answers /help itself, so topics are accepted and dropped. */
public final class BasaltHelpMap extends HelpMapBase {
    @Override
    public HelpTopic getHelpTopic(String name) {
        return null;
    }

    @Override
    public Collection<HelpTopic> getHelpTopics() {
        return List.of();
    }

    @Override
    public void addTopic(HelpTopic topic) {}

    @Override
    public void clear() {}

    @Override
    public void registerHelpTopicFactory(Class<?> commandClass, HelpTopicFactory<?> factory) {}

    @Override
    public List<String> getIgnoredPlugins() {
        return List.of();
    }
}
