"use client";

import { CodeTabs } from "@/components/ui/code-tabs";

const DOCKER_BUILD_TABS = [
  {
    label: "C++20 & C",
    language: "docker",
    code: `# Multi-stage Docker build for C++20 / C consuming MiniGit SDK
FROM ubuntu:24.04 AS build

# Install compiler and build dependencies
RUN apt-get update && apt-get install -y \\
    build-essential libssl-dev zlib1g-dev libcurl4-openssl-dev

# Copy MiniGit SDK headers and compiled archive directly from official image
COPY --from=minigit:latest /usr/local/include/minigit /usr/local/include/minigit
COPY --from=minigit:latest /usr/local/lib/libminigit_sdk.a /usr/local/lib/libminigit_sdk.a

WORKDIR /app
COPY . .
RUN g++ -std=c++20 main.cpp -lminigit_sdk -lssl -lcrypto -lz -lcurl -o my_service

# Minimal runtime container
FROM ubuntu:24.04 AS runtime
RUN apt-get update && apt-get install -y libssl3 zlib1g libcurl4 && rm -rf /var/lib/apt/lists/*
COPY --from=build /app/my_service /usr/local/bin/my_service

WORKDIR /data
VOLUME ["/data"]
ENTRYPOINT ["/usr/local/bin/my_service"]`,
  },
  {
    label: "Rust",
    language: "docker",
    code: `# Multi-stage Docker build for Rust application consuming MiniGit SDK
FROM rust:1.75 AS builder

WORKDIR /workspace
# Copy the MiniGit Rust SDK crate and your application
COPY sdks/rust ./sdks/rust
COPY my_rust_app ./my_rust_app

WORKDIR /workspace/my_rust_app
RUN cargo build --release

FROM debian:bookworm-slim
COPY --from=builder /workspace/my_rust_app/target/release/my_rust_app /usr/local/bin/

WORKDIR /data
VOLUME ["/data"]
ENTRYPOINT ["/usr/local/bin/my_rust_app"]`,
  },
  {
    label: "Python",
    language: "docker",
    code: `# Dockerfile for containerized Python application consuming MiniGit SDK
FROM python:3.11-slim

WORKDIR /app

# Copy the self-contained MiniGit Python SDK module
COPY sdks/python/minigit_sdk /app/minigit_sdk
COPY main.py requirements.txt /app/

WORKDIR /data
VOLUME ["/data"]
ENTRYPOINT ["python", "/app/main.py"]`,
  },
  {
    label: "TypeScript / Node",
    language: "docker",
    code: `# Multi-stage Dockerfile for TypeScript / Node.js application
FROM node:20-alpine AS builder

WORKDIR /app
# Copy the MiniGit JavaScript / TypeScript SDK
COPY sdks/js ./sdks/js
COPY package.json tsconfig.json index.ts ./

RUN npm install && npm run build

FROM node:20-alpine AS runtime
WORKDIR /app
COPY --from=builder /app/dist ./dist
COPY --from=builder /app/sdks/js ./sdks/js
COPY --from=builder /app/node_modules ./node_modules

WORKDIR /data
VOLUME ["/data"]
CMD ["node", "/app/dist/index.js"]`,
  },
  {
    label: "Go",
    language: "docker",
    code: `# Multi-stage Dockerfile for Go application consuming MiniGit SDK
FROM golang:1.22-alpine AS builder

WORKDIR /app
# Copy the MiniGit Go SDK module
COPY sdks/go ./sdks/go
COPY main.go go.mod ./

RUN go build -o /app/service main.go

FROM alpine:latest
COPY --from=builder /app/service /usr/local/bin/service

WORKDIR /data
VOLUME ["/data"]
ENTRYPOINT ["/usr/local/bin/service"]`,
  },
  {
    label: "Java",
    language: "docker",
    code: `# Dockerfile for Java application consuming MiniGit SDK
FROM maven:3.9-eclipse-temurin-17 AS builder

WORKDIR /app
COPY sdks/java ./sdks/java
COPY pom.xml src ./

RUN mvn clean package -DskipTests

FROM eclipse-temurin:17-jre-alpine
COPY --from=builder /app/target/app.jar /usr/local/app.jar

WORKDIR /data
VOLUME ["/data"]
ENTRYPOINT ["java", "-jar", "/usr/local/app.jar"]`,
  },
];

export function DockerSdkTabs() {
  return <CodeTabs tabs={DOCKER_BUILD_TABS} />;
}
