package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import org.basaltmc.bukkit.gen.HelpMapBase;
import org.bukkit.Bukkit;
import org.bukkit.command.Command;
import org.bukkit.help.GenericCommandHelpTopic;
import org.bukkit.help.HelpTopic;
import org.bukkit.help.HelpTopicFactory;

/** The help topics plugins register, plus one generic topic for every command the server knows. */
public final class BasaltHelpMap extends HelpMapBase {
    private final Map<String, HelpTopic> topics = new ConcurrentHashMap<>();
    private final Map<Class<?>, HelpTopicFactory<?>> factories = new ConcurrentHashMap<>();

    private static String key(String name) {
        return name.toLowerCase(Locale.ROOT);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private HelpTopic commandTopic(Command command) {
        for (Class<?> type = command.getClass(); type != null; type = type.getSuperclass()) {
            HelpTopicFactory factory = factories.get(type);
            if (factory != null) {
                HelpTopic made = factory.createTopic(command);
                if (made != null) {
                    return made;
                }
            }
        }
        return new GenericCommandHelpTopic(command);
    }

    @Override
    public HelpTopic getHelpTopic(String name) {
        if (name == null) {
            return null;
        }
        HelpTopic held = topics.get(key(name));
        if (held != null) {
            return held;
        }
        if (name.startsWith("/")) {
            Command command = Bukkit.getCommandMap().getCommand(name.substring(1));
            return command == null ? null : commandTopic(command);
        }
        return null;
    }

    @Override
    public Collection<HelpTopic> getHelpTopics() {
        Map<String, HelpTopic> all = new LinkedHashMap<>(topics);
        for (Command command : new ArrayList<>(Bukkit.getCommandMap().getKnownCommands().values())) {
            all.computeIfAbsent("/" + key(command.getName()), ignored -> commandTopic(command));
        }
        return List.copyOf(all.values());
    }

    @Override
    public void addTopic(HelpTopic topic) {
        topics.put(key(topic.getName()), topic);
    }

    @Override
    public void clear() {
        topics.clear();
    }

    @Override
    public void registerHelpTopicFactory(Class<?> commandClass, HelpTopicFactory<?> factory) {
        if (!Command.class.isAssignableFrom(commandClass) && !org.bukkit.command.CommandExecutor.class
            .isAssignableFrom(commandClass)) {
            throw new IllegalArgumentException("commandClass must implement either Command or CommandExecutor!");
        }
        factories.put(commandClass, factory);
    }

    @Override
    public List<String> getIgnoredPlugins() {
        return List.of();
    }
}
