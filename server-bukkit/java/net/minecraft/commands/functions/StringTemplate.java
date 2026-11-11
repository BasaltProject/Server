// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.functions;
import java.util.ArrayList;
import java.util.List;
public record StringTemplate(List<String> segments, List<String> variables) {
    public static StringTemplate fromString(String input) {
        List<String> segments = new ArrayList<>();
        List<String> variables = new ArrayList<>();
        int consumed = 0;
        int dollar = input.indexOf('$');
        while (dollar != -1) {
            if (dollar == input.length() - 1 || input.charAt(dollar + 1) != '(') {
                dollar = input.indexOf('$', dollar + 1);
                continue;
            }
            segments.add(input.substring(consumed, dollar));
            int close = input.indexOf(')', dollar + 1);
            if (close == -1) {
                throw new IllegalArgumentException("Unterminated macro variable");
            }
            String name = input.substring(dollar + 2, close);
            if (!isValidVariableName(name)) {
                throw new IllegalArgumentException("Invalid macro variable name '" + name + "'");
            }
            variables.add(name);
            consumed = close + 1;
            dollar = input.indexOf('$', consumed);
        }
        if (consumed == 0) {
            throw new IllegalArgumentException("No variables in macro");
        }
        if (consumed != input.length()) {
            segments.add(input.substring(consumed));
        }
        return new StringTemplate(List.copyOf(segments), List.copyOf(variables));
    }
    public static boolean isValidVariableName(String variable) {
        return variable.chars().allMatch(c -> Character.isLetterOrDigit(c) || c == '_');
    }
    public String substitute(List<String> arguments) {
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < variables.size(); i++) {
            out.append(segments.get(i)).append(arguments.get(i));
            CommandFunction.checkCommandLineLength(out);
        }
        if (segments.size() > variables.size()) {
            out.append(segments.getLast());
        }
        CommandFunction.checkCommandLineLength(out);
        return out.toString();
    }
}
