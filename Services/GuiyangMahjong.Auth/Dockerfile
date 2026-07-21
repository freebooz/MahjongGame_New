FROM mcr.microsoft.com/dotnet/sdk:10.0 AS build
WORKDIR /src
COPY Services/GuiyangMahjong.Auth/GuiyangMahjong.Auth.csproj Services/GuiyangMahjong.Auth/
RUN dotnet restore Services/GuiyangMahjong.Auth/GuiyangMahjong.Auth.csproj
COPY Contracts/ Contracts/
COPY Services/GuiyangMahjong.Auth/ Services/GuiyangMahjong.Auth/
RUN dotnet publish Services/GuiyangMahjong.Auth/GuiyangMahjong.Auth.csproj -c Release -o /app --no-restore

FROM mcr.microsoft.com/dotnet/aspnet:10.0
RUN apt-get update \
    && apt-get install --yes --no-install-recommends curl \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /app .
ENV ASPNETCORE_URLS=http://+:8080
EXPOSE 8080
USER $APP_UID
ENTRYPOINT ["dotnet", "GuiyangMahjong.Auth.dll"]
