package io.minigit.sdk;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class MiniGitClient {
    private final Path repoPath;
    private final String binaryPath;

    public MiniGitClient(Path repoPath, String binaryPath) {
        this.repoPath = repoPath.toAbsolutePath().normalize();
        this.binaryPath = (binaryPath != null && !binaryPath.isEmpty()) ? binaryPath : resolveBinary();
    }

    public static String resolveBinary() {
        String env = System.getenv("MINIGIT_BIN");
        if (env != null && !env.isEmpty()) {
            return env;
        }
        String isWindows = System.getProperty("os.name").toLowerCase().contains("win") ? ".exe" : "";
        return "minigit" + isWindows;
    }

    public static MiniGitClient init(Path path) throws Exception {
        Files.createDirectories(path);
        MiniGitClient client = new MiniGitClient(path, null);
        client.run("init");
        return client;
    }

    public static MiniGitClient open(Path path) throws Exception {
        MiniGitClient client = new MiniGitClient(path, null);
        if (!Files.exists(client.repoPath.resolve(".minigit"))) {
            throw new IllegalArgumentException("Not a valid MiniGit repository: " + path);
        }
        return client;
    }

    private String run(String... args) throws Exception {
        List<String> command = new ArrayList<>();
        command.add(binaryPath);
        command.addAll(Arrays.asList(args));

        ProcessBuilder pb = new ProcessBuilder(command);
        pb.directory(repoPath.toFile());
        pb.redirectErrorStream(true);

        Process p = pb.start();
        StringBuilder out = new StringBuilder();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(p.getInputStream()))) {
            String line;
            while ((line = reader.readLine()) != null) {
                out.append(line).append("\n");
            }
        }
        int code = p.waitFor();
        if (code != 0) {
            throw new RuntimeException("MiniGit command failed (" + code + "): " + out.toString().trim());
        }
        return out.toString().trim();
    }

    public void add(String... paths) throws Exception {
        List<String> args = new ArrayList<>();
        args.add("add");
        args.addAll(Arrays.asList(paths));
        run(args.toArray(new String[0]));
    }

    public void addAll() throws Exception {
        add(".");
    }

    public String commit(String message, String author) throws Exception {
        List<String> args = new ArrayList<>();
        args.add("commit");
        args.add("-m");
        args.add(message);
        if (author != null && !author.isEmpty()) {
            args.add("--author");
            args.add(author);
        }
        return run(args.toArray(new String[0]));
    }

    public String status() throws Exception {
        return run("status");
    }

    public String log() throws Exception {
        return run("log");
    }

    public String currentBranch() {
        Path head = repoPath.resolve(".minigit").resolve("HEAD");
        try {
            if (Files.exists(head)) {
                String content = Files.readString(head).trim();
                if (content.startsWith("ref: refs/heads/")) {
                    return content.substring("ref: refs/heads/".length());
                }
            }
        } catch (Exception ignored) {}
        return "HEAD (detached)";
    }

    public void createBranch(String branchName) throws Exception {
        run("branch", branchName);
    }

    public void switchBranch(String branchName) throws Exception {
        run("switch", branchName);
    }

    public Path getRepoPath() {
        return repoPath;
    }
}
