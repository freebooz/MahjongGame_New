FROM mcr.microsoft.com/dotnet/sdk:10.0 AS build
WORKDIR /src
COPY Services/GuiyangMahjong.Lobby/GuiyangMahjong.Lobby.csproj Services/GuiyangMahjong.Lobby/
RUN dotnet restore Services/GuiyangMahjong.Lobby/GuiyangMahjong.Lobby.csproj
COPY Contracts/ Contracts/
COPY Services/GuiyangMahjong.Lobby/ Services/GuiyangMahjong.Lobby/
RUN dotnet publish Services/GuiyangMahjong.Lobby/GuiyangMahjong.Lobby.csproj -c Release -o /app --no-restore

FROM mcr.microsoft.com/dotnet/aspnet:10.0
WORKDIR /app
COPY --from=build /app .
ENV ASPNETCORE_URLS=http://+:8080
EXPOSE 8080
USER $APP_UID
ENTRYPOINT ["dotnet", "GuiyangMahjong.Lobby.dll"]
