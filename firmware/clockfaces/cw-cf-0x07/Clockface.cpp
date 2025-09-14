
#include "Clockface.h"
#include "localcanvas.h"

unsigned long lastMillis = 0;

// TODO document size
static DynamicJsonDocument doc(32768);

Clockface::Clockface(Adafruit_GFX *display)
{
  _display = display;
  Locator::provide(display);
}

void Clockface::setup(CWDateTime *dateTime)
{
  this->_dateTime = dateTime;
  drawSplashScreen(0xFFE0, "Downloading");

  if (deserializeDefinition()) {
    clockfaceSetup();
  }
}

void Clockface::drawSplashScreen(uint16_t color, const char *msg) {
  
  Locator::getDisplay()->fillRect(0, 0, 64, 64, 0);
  Locator::getDisplay()->drawBitmap(19, 18, CW_ICON_CANVAS, 27, 32, color);
  
  StatusController::getInstance()->printCenter("- Canvas -", 7);
  StatusController::getInstance()->printCenter(msg, 61);
}

void Clockface::update()
{
  // Render animation
  clockfaceLoop();

  // Update Date/Time - Using a fixed interval (1000 milliseconds)
  if (millis() - lastMillis >= 1000)
  {
    refreshDateTime();
    lastMillis = millis();
  }
}

void Clockface::setFont(const char *fontName)
{

  if (strcmp(fontName, "picopixel") == 0)
  {
    Locator::getDisplay()->setFont(&Picopixel);
  }
  else if (strcmp(fontName, "square") == 0)
  {
    Locator::getDisplay()->setFont(&atariFont);
  }
  else if (strcmp(fontName, "big") == 0)
  {
    Locator::getDisplay()->setFont(&hour8pt7b);
  }
  else if (strcmp(fontName, "medium") == 0)
  {
    Locator::getDisplay()->setFont(&minute7pt7b);
  }
  else if (strcmp(fontName, "carto") == 0)
  {
    Locator::getDisplay()->setFont(&cartographer3pt7b);
  }

  else
  {
    Locator::getDisplay()->setFont();
  }
}

void Clockface::renderText(String text, JsonVariantConst value)
{
  int16_t x1, y1;
  uint16_t w, h;

  setFont(value["font"].as<const char *>());

  Locator::getDisplay()->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  // BG Color
  Locator::getDisplay()->fillRect(
      value["x"].as<const uint16_t>() + x1,
      value["y"].as<const uint16_t>() + y1,
      w + 4, //Problems with large fonts; when changing from the number 0 to the number 1, it remained blurry. I added 4 for major clean 
      h,
      value["bgColor"].as<const uint16_t>());

  Locator::getDisplay()->setTextColor(value["fgColor"].as<const uint16_t>());
  Locator::getDisplay()->setCursor(value["x"].as<const uint16_t>(), value["y"].as<const uint16_t>());
  Locator::getDisplay()->print(text);
}

void Clockface::refreshDateTime()
{

  JsonArrayConst elements = doc["setup"].as<JsonArrayConst>();
  for (JsonVariantConst value : elements)
  {
    const char *type = value["type"].as<const char *>();

    if (strcmp(type, "datetime") == 0)
    {
      renderText(_dateTime->getFormattedTime(value["content"].as<const char *>()), value);
    }
  }
}

void Clockface::clockfaceSetup()
{

  // Clear screen
  Locator::getDisplay()->fillRect(0, 0, 64, 64, doc["bgColor"].as<const uint16_t>());

  delay = doc["delay"].as<const uint16_t>();

  // Draw static elements
  renderElements(doc["setup"].as<JsonArrayConst>());

  // Draw Date/Time
  refreshDateTime();

  // Create sprites
  createSprites();
}

void Clockface::createSprites()
{
  JsonArrayConst elements = doc["loop"].as<JsonArrayConst>();
  uint8_t width = 0;
  uint8_t height = 0;

  for (JsonVariantConst value : elements)
  {
    const char *type = value["type"].as<const char *>();

    if (strcmp(type, "sprite") == 0)
    {
      uint8_t ref = value["sprite"].as<const uint8_t>();

      std::shared_ptr<CustomSprite> s = std::make_shared<CustomSprite>(value["x"].as<const int8_t>(), value["y"].as<const int8_t>());

      getImageDimensions(doc["sprites"][ref][0]["image"].as<const char *>(), width, height);

      s.get()->_spriteReference = value["sprite"].as<const uint8_t>();
      s.get()->_totalFrames = doc["sprites"][ref].size();
      s.get()->setDimensions(width, height);
      sprites.push_back(s);
    }
  }
}

void Clockface::handleSpriteAnimation(std::shared_ptr<CustomSprite>& sprite) {
    uint8_t totalFrames = sprite->_totalFrames;
    uint32_t loopDelay = doc["loop"][sprite->_spriteReference]["loopDelay"].as<uint32_t>() ?: delay;
    uint16_t frameDelay = doc["loop"][sprite->_spriteReference]["frameDelay"].as<uint16_t>() ?: delay;

    if (millis() - sprite->_lastMillisSpriteFrames >= frameDelay && sprite->_currentFrameCount < totalFrames) {
        sprite->incFrame();

        // handle sprite movement
        handleSpriteMovement(sprite);

        // Render the frame of the sprite
        renderImage(doc["sprites"][sprite->_spriteReference][sprite->_currentFrame]["image"].as<const char *>(), sprite->getX(), sprite->getY());

        sprite->_currentFrameCount += 1;
        sprite->_lastMillisSpriteFrames = millis();
    }

    if (millis() - sprite->_lastResetTime >= loopDelay) {
        unsigned long currentMillis = millis();
        unsigned long currentSecond = _dateTime->getSecond();

        if ((currentSecond * 1000) % loopDelay == 0) {
            sprite->_currentFrameCount = 0;
            sprite->_lastResetTime = currentMillis;
        }
    }
}

void Clockface::handleSpriteMovement(std::shared_ptr<CustomSprite>& sprite) {
    unsigned long moveStartTime = doc["loop"][sprite->_spriteReference]["moveStartTime"].as<unsigned long>() ?: 1;
    unsigned long moveDuration = doc["loop"][sprite->_spriteReference]["moveDuration"].as<unsigned long>() ?: 0;
    int8_t moveInitialX = doc["loop"][sprite->_spriteReference]["x"].as<int8_t>() ?:0;
    int8_t moveInitialY = doc["loop"][sprite->_spriteReference]["y"].as<int8_t>() ?: 0;
    int8_t moveTargetX = doc["loop"][sprite->_spriteReference]["moveTargetX"].as<int8_t>() ?: -1;
    int8_t moveTargetY = doc["loop"][sprite->_spriteReference]["moveTargetY"].as<int8_t>() ?: -1;
    bool shouldReturnToOrigin = doc["loop"][sprite->_spriteReference]["shouldReturnToOrigin"].as<bool>() ?: false;

    // Check if the sprite is moving
    if (sprite->isMoving()) {
        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - sprite->_moveStartTime;
        float progress = (static_cast<float>(elapsedTime) / sprite->_moveDuration);

        int8_t oldX = sprite->getX();
        int8_t oldY = sprite->getY();
        int8_t newX = sprite->lerp(sprite->_moveInitialX, sprite->_moveTargetX, progress);
        int8_t newY = sprite->lerp(sprite->_moveInitialY, sprite->_moveTargetY, progress);
        int8_t originX = min(oldX, newX);
        int8_t originY = min(oldY, newY);
        int8_t drawWidth = sprite->getWidth() + max(oldX, newX) - originX;
        int8_t drawHeight = sprite->getHeight() + max(oldY, newY) - originY;

        // Erase the previous position
        Locator::getDisplay()->fillRect(
            originX,
            originY,
            drawWidth,
            drawHeight,
            doc["bgColor"].as<const uint16_t>());

        if (progress <= 1) {
            // Update the sprite's position
            sprite->setX(newX);
            sprite->setY(newY);

        } else if (sprite->shouldReturnToOrigin()) {
            // Movement is complete
            sprite->setX(sprite->_moveTargetX);
            sprite->setY(sprite->_moveTargetY);

            if (!sprite->_isReversing) {
                sprite->reverseMoving(moveInitialX, moveInitialY);
            }
        } else {
            sprite->stopMoving();
        }
    }

    if ((moveDuration > 0 && (moveTargetX > -1 || moveTargetY > -1)) && (millis() - sprite->_lastResetMoveTime >= moveStartTime)) {
        unsigned long currentMillis = millis();
        unsigned long currentSecond = _dateTime->getSecond();

        if ((currentSecond * 1000) % moveStartTime == 0) {
            sprite->_lastResetMoveTime = currentMillis;
            sprite->startMoving(moveTargetX, moveTargetY, moveDuration, shouldReturnToOrigin);
        }
    }
}

void Clockface::clockfaceLoop() {
    if (sprites.empty()) {
        return;
    }

    for (auto& sprite : sprites) {
        handleSpriteAnimation(sprite);
    }
}

void Clockface::renderElements(JsonArrayConst elements)
{
  for (JsonVariantConst value : elements)
  {
    const char *type = value["type"].as<const char *>();

    if (strcmp(type, "text") == 0)
    {
      renderText(value["content"].as<const char *>(), value);
    }
    else if (strcmp(type, "fillrect") == 0)
    {
      Locator::getDisplay()->fillRect(
          value["x"].as<const uint16_t>(),
          value["y"].as<const uint16_t>(),
          value["width"].as<const uint16_t>(),
          value["height"].as<const uint16_t>(),
          value["color"].as<const uint16_t>());
    }
    else if (strcmp(type, "rect") == 0)
    {
      Locator::getDisplay()->drawRect(
          value["x"].as<const uint16_t>(),
          value["y"].as<const uint16_t>(),
          value["width"].as<const uint16_t>(),
          value["height"].as<const uint16_t>(),
          value["color"].as<const uint16_t>());
    }
    else if (strcmp(type, "line") == 0)
    {
      Locator::getDisplay()->drawLine(
          value["x"].as<const uint16_t>(),
          value["y"].as<const uint16_t>(),
          value["x1"].as<const uint16_t>(),
          value["y1"].as<const uint16_t>(),
          value["color"].as<const uint16_t>());
    }
    else if (strcmp(type, "image") == 0)
    {
      renderImage(value["image"].as<const char *>(), value["x"].as<const uint8_t>(), value["y"].as<const uint8_t>());
    }
  }
}

bool Clockface::deserializeDefinition()
{
  WiFiClientSecure client;

  //WiFiClient client;
  //ClockwiseHttpClient::getInstance()->httpGet(&client, "raw.githubusercontent.com", "/jnthas/clock-club/v1/pac-man.json", 443);
  //ClockwiseHttpClient::getInstance()->httpGet(&client, "192.168.3.19", "/nyan-cat.json", 4443);
  //ClockwiseHttpClient::getInstance()->httpGet(&client, "192.168.1.2", "/clock-club.json", 4443);

  if (ClockwiseParams::getInstance()->canvasServer.isEmpty() || ClockwiseParams::getInstance()->canvasFile.isEmpty()) {
    drawSplashScreen(0xC904, "Params werent set");
    return false;
  }


  String server = ClockwiseParams::getInstance()->canvasServer;
  String file = String("/" + ClockwiseParams::getInstance()->canvasFile + ".json");
  uint16_t port = 4443;
//  const char* peppe = "{\"name\": \"Donkey Kong\",\"version\": 1,\"author\": \"jnthas\",\"bgColor\": 0,\"delay\": 1000,\"setup\": [{\"type\": \"image\",\"x\": 0,\"y\": 0,\"image\": \"iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAYAAACqaXHeAAACW0lEQVR42u2bLXaDQBDHJ33ICg5SkSNURERUICoic4TKHKGyR6iMqEBUVCByBEQPsqKS91KTDTBhP9mFzeM/DkJ4szO/+dgPVkR0pgVLFvPlxzzMe3Yino4PtHBZxQgB7vntqexd/zwXg/8zP\nReDBBAQkgCT57lID9s+F4MEEOBDgKj1sary6OPTS+/67/dbe9+WmHxdgIBJ+gDued/YzrKs9z553TSNVQ5Q6eVDAggYsqTJ4r4iY5uTJK9VsT+285QkDhGCKkBEZ5PnpQVts71tjjB1gq7vUempIwEEiLo8u8a8r+djia/++boAAVmMjB+rioQiT+on6hIErI65/2xQNX8PnSNM1W\ncMcSBgDAFTV4cYuWXxBNyVAbanMjhlyAEhc0Dsur34EJidALkaG2rHJ6T46oa5gIt1XX+bQrjHXUlADtDlAO7dY357r/nczIvwvnIiBAS45IDuWuFOSO9XF8+/ExHR11s16wAkgdn+cLmzudEdBIypAnIFtbU0JeF5Ka0eh76eF72RA3wIGFpf55Z+/dikNRIFmaa9AhBgO/u6eny\nf9oBaPStUAbTCMAAMAAOM6gOIiGhd3PUA0Qe4EMCt1SWBd4KpCu8E+Vg4CSDANm54J5jKbLCVw2AnaNqbAAGmDCqXALNrNehbem4SOJlyjdD2VCtWhYdWhXVxw09bprYqrPI8+gAXAkxEdK3Z3SeYYs9waK/CpCNygC6EupbzOXEpaNwXG77ie7KdkwICVL2/LpOmdFZYNX9R6cif\nQxUQdel9RojH/hxVwLbjQxVQEUAL/3z+H5OIJ2iGrWalAAAAAElFTkSuQmCC\",\"id\": \"suhi8u\"},{\"type\": \"datetime\",\"x\": 15,\"y\": 44,\"content\": \"H:i\",\"font\": \"square\",\"fgColor\": 41088,\"bgColor\": 64776, \"id\": \"luxs89\"},{\"type\": \"datetime\",\"x\": 23,\"y\": 60,  \"content\": \"m/d\",\"font\": \"picopixel\",\"fgColor\": 64776,  \"bgColor\": 41088,\"id\": \"mthjm5\"}],\"sprites\": [],\"loop\": []}";
//  const char* peppe = "{\"name\": \"Vespa\",\"version\": 1,\"author\": \"Tato\",\"bgColor\": 2776,\"delay\": 1000,\"setup\": [{\"type\": \"image\",\"x\": 0,\"y\": 56,\"image\": \"iVBORw0KGgoAAAANSUhEUgAAAEAAAAAICAYAAABJYvnfAAAAAXNSR0IB2cksfwAAAARnQU1BAACxjwv8YQUAAAAgY0hSTQAAeiYAAICEAAD6AAAAgOgAAHUwAADqYAAAOpgAABdwnLpRPAAAAvBJREFUSMdNlUGW6zoIRG+987fW6YFgKUmvJc5SBAM7i+MPkJ0McmJbQJWgAAkKAESpEHC7/a6vRSGOY18m6r8qQJyuK8Blcrv99tcS+/tAVdQyl+B0b6wb5+F+HO3/c6MASewLW9U4An5+b6hYsaFZnwTg9/ZDlUDFcRxQWrgd4ff3tm4GisiL/+u1cew7EQGAuRERi7B4PTf2905EAoUNIzOpKoTYXhvHfjAzEIUNJzIu4NdrYz8OMqL9za5YVbBtG8f7TcakSrgZMwLU8Z/PjfdxELn42eIHULX4v5kxm/vM9q3mtu8HERMzIyMpwT9USLC9NmpluQBzJ2asSsBr21ohSxlmTkZ0JiW213P5dhAzb/Jfl+/nomolZ3ZyqsS2bZeoqoS7MXOiBmTbnu2rolbyIyanBLftdSnRzDuxahU+X89LHW5OZLYCC/65OVXwuN9bSOrfXJnyYQA8HndUfSggIxhmuHmf3x+XxEUREbgbZoYE9/v9kj2IyMnw07/4ezzQkqqAmI0/hkHB/f5AEkKoYGZgw7BhQPH4u0N1G2dO3AY+HAGPxx9ndSMCH6PjAv9iTtwHbtatCNdFQcwMxgm0zsfwqyFnTGwMhjm6OussdjEjsTFw968+7aqrxMyJmTPcqc9AOkcQkYG7Y26Xehq650pkJ8rMr1bpAFpFHJgNPlOrESIm5oP/EEvqwn2QkZ8+uKqVqHomzIgly7MdaKkD7t3TPROWHChmRreFGzHjM0CX5CImQpiPq6e1hiCImBMkzEbLd7XNpaYIED2TFhfU+I2ta55J37iBImKFOif1Od31ydi6yGqpxX1VetX8nBW1ZKSvwVxX4etrY2gppo17s0Cda+JrXTR8fVHTtWU+ZuudrxWzbOpMyGdRfJ4iowQlqTKilmVJVEZWZpTWtzjP1fYRUZFZpyRWMpe9KjIrIktqdX+e1f6ZC7PxIrMkFdJln2dMUZmfWCd+ZnNq/nl2UZ9FLr7rblfcdSbqf4r6RxSO716fAAAAAElFTkSuQmCC\",\"id\": \"strada\"},{\"type\": \"image\",\"x\": 0,\"y\": 18,\"image\":\"iVBORw0KGgoAAAANSUhEUgAAAAwAAAAMCAYAAABWdVznAAAAAXNSR0IArs4c6QAAAPRJREFUKFNdUlF2gkAQy/DhegI5Selr9SZF6dGkepSiT3sR8QTiB9OXmWUB8wGz7CSbzCKhbLQ7riEAFIJQ/iLBP/pDBMoXIKp9D2VFimTW70QyRoLxSNc+ViZHGpCJIGwbPA9rLCLZdlVVq2uLfZFba3W583TsixWWuzMeP59+qijCVwPZXW5aF3nUdgPfJrACfU6tLsrGApif0cy8ThkgEJFXgg/EVadwuUhQnTX53gt8eiFZipIMT3L9noN1/ZanDCwkE4ZudbiROgat/lpwENNJ0M4sdNie0B0+UgA2EMNl005cC2Oav+64Gf4FW48YZ/gPXCaDZEiKVr8AAAAASUVORK5CYIIA\",\"id\": \"nuvola\"},{\"type\": \"datetime\",\"x\": 17,\"y\": 28,\"content\": \"H:i\",\"font\": \"carto\",\"fgColor\": 65535,\"bgColor\": 2776,\"id\": \"10ygfp\"},{\"type\": \"datetime\",\"x\": 3,\"y\": 2,\"content\": \"D d M\",\"font\": \"\",\"fgColor\": 65535,\"bgColor\": 2776,\"id\": \"84wq6j\"}],\"sprites\": [[{\"image\": \"iVBORw0KGgoAAAANSUhEUgAAAEAAAAAUCAYAAAA9djs/AAAAAXNSR0IArs4c6QAAAwhJREFUWEelWDtyGzEMBZvoDJlRlb2Aq7jQESKPVaZQLmLX8UXizNjdeuLcIFskaZQDyFWinEFpNgOSWBLgDyttodFqQRB4eHjEyiy2wwgAgB8Gv9CFN/aJ8jIjwMg8RAsT7wqnp6zhbicPMpfo3hAAtLS87cyA1AB6vy37zHNdRHUrBkDJVLeRg3COrYIGzqcBQIKddU0AhgjdN2NGGIP3xXaY9hlhRIMZ+44AxkytUwNDhpGAV6FtElCLPWyBywov/DRdv3bZeyf7zTO82g6ZtEsh41qZNE+9yQpcjtgV+EPrcxEw4GYB4VBJAMAw9psvEDNhBgXOM40S0IDmmNtgqBRnAZJ506+tdmPlu35tHaLblwkExSYi7XhFulrjz9LBNr/G+iTl8bg5BhiA/TUB4MXMALxcPwsm6MKp9ihVQENXZlPZu+iLtK3AEhRXYkCeu7p2qMJSPL7Szo61KBtPRsnLPaeT4KABlbJheyy239r9NkMBZPruviRzMxwLxS9qhAfTA+Duapg5EAZ3IGcmPplAri9PbKDoWM6ko2mlikYVGZAGe347tAAoNEWpO71YBa/H+5X9jU6w3H5kQ3YWADfukGBE+5F2RI8mJhTqoh2eZiXbmDDR17/7FTz8Xdqo3r/+bUHAZOPj/Ph5BQ8HbpOZA1xm4VhMC5CCQDYR5kLtWTX8M/tb/F3d6ry2mCglTy4QBPwtBiO1+QOme/KToCBADQA3J8gj0jOSDZphwqSnaRdrOZNBxzP0+IkDcPv9Aj5e7gSVowRHgNufF3D3dhcmQcKUEk8AEGJTA0FTyLoetNTCw2k89Q9LwKTxwsQx1BsLwi/2Tk82d5c72/B4b7qnK/YytN98ha5/53ZQKGy7HUrJ8BcnBlr1vwVnSWL2eFjCzQ9ZcV6CAEjMCp9i11/huaYpmrWR6aCAhrFZujmtzq36Tz3fMhTh5HCtDEKlro1pEb6XmKCKsTnhpaL36BVfXzqPBi1g7wIxUlMw/l8IsUMtIQvCh0FFKO5HfygyxVeg2+ri/5bTgJEr6liAAAAAAElFTkSuQmCC\",\"id\": \"cespu\"},{\"image\": \"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\",\"id\": \"vesp1\"},{\"image\": \"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\",\"id\": \"vesp2\"},{\"image\": \"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\",\"id\": \"vesp3\"},{\"image\": \"iVBORw0KGgoAAAANSUhEUgAAAEAAAAAUCAYAAAA9djs/AAAAAXNSR0IArs4c6QAABo1JREFUWEeFWMtOW1cU3WcQe9pZGpMMsNVpIB1gKKYqMAWFR5EiAV8QMq6SjEM/IPwBRkEKTwWGQNpAsdUBRJ3Gl0kw5RMgg1vt59n32qiegO/jnL3XXnvtdRyKCycp2Af/DZBCCt/qIwAhhTQNdKWwcAIAfB8//FIKEIL8SQHSwLfdirx0XBdXy+9n3zO34l7uBbeaXtXndFMJIARIMTwXo8VCjwQOlwHgRPAF/NyuDcPXz7/Ao76P0BobpNvlowYU5xEE+QSXsORoISNwEBC/XLL41YNEEd4NHC3AAGOwmosCbHhLHTgBhyK9I9fk/QgXRagAyD6y0G29RgD0PP4ICQKA6YQAvYenlBSzQeqK8UkekRmZMBwDIhssTJck3+W9KGlNR1ll7LqbHZF+UmF6NAJJ/xOaGn8IKW6qF4sLx3TzZm2YEMLsEAR857fveui1jc0tuLdwTPdpaQMhhdRawnUM7ZAlvwWiSPiE85zvoHHuAdd2GWhy7eibBcOkZSs7E0xUSSKZ2ofCIibHDyAQyegQlA8bllFh4ZOk051eXis8U5SOtKF2ArFOsvdiIPHk1/IKpPpjCkNrRvbQHtYWUgTHWGohDwAJAwAkUx+gKKJ3U6/BxdgQ0Z+vCckCwO0qskTJwxtjZQOtk/LzvgqZivhaRSC1nYiVxjFBjP44QeDm5GJ1qTYXVp4QPaKkjW0phPLuBO11MbUH5Z0JKU0Krak9SiQZr0LvQaMjmZv6MCwtLUkRvOCpIARYefuW9MJroVLP92IktLZKVHQNuGO6eIH7vxaJHc9bqSbgv8SAAJA8RQAmPTx0DaeATQAnJtgac9OzEXmbCnF0bmxts1Z0sCCOT02+Q/C6VJWf7XxXE7KxJz3NcDr25N9HNpR3JkQTfVPyPq2pfbgYHxQGHDsXAHBbH4YfH/dFR6DUkj7EYM7++cwASEsQWa1TMpJkYmp06SZs1L9CadON/Njz0yOKu5eXOC5TZgArOauDzlau/hAh2HvUgMJ8FD4KgTSg5noyCqnSG9unLO2Tr3CmLfygtBGoGhD729ohPyUy7eBvxlZi7sRZpMIpLeCSl2olU3twMT5EoCAAWBkWNTdT0TTVa9AaR7PEdFNwCEwBqvcQNeTY+YUuY9Hmct5RiknT1pX8bPzKdx7mOmqzPoFhcPG59rIp0JreY83FRNQVojaMVaFy0CB9aNF0wFbgjXRhtM0IArMyJc1QSWAjFaBycGrewdrSF0jeNW9hhZRaOfWm+HVOSwPjNWxL/EqO1Rs05wLxGVFCKOK4xxbAarfGke4oYDJLhTLlwyZVvbKNEwJ1YQ+KiyfOZwPcrI5AQixgy5yMVnnOp9g+TbgYG6QxWpg/cUZVSSjJuJ7I1k/JG+PKCKckiomtXz2kW89KX4mtN6s1KCx+Ig0igNZq8K6NZg7g2QN+hgBPpNeVtqy0PODxT/nwNI5I4BGpPgEfxIUxaTRLuBZ/4iQvH53Sdbx/b/5YwMkaEwUssiMHg7OweXOEyb+T5BUcTBAB0URxbK9fPcqc1PAeM07zpZ7lUqAAmiEnEBpQIZ/AxgdFklBeq8EFJd+kkalOjSwxPcwtQSxAQVw8iXbdaN75T8YmCZuU1trCSic0ZASAvPSq+QSWq2euTTtd0qu/++H3gXNuAez/F8+XqFf/bV/C96USrKys8ElQpkPlsGFTlVcWzqqnDgFaowiAP1nGQxQyoCJtYOm6w42eIZgB7CZ9pbtNAIwARXj9qgdeN/vp1TfVcwrvZbMflqvnNkL0GlZ8eeCMivi6+QRCZXciTZ7uw/MXS3B92Ybkyxfo/aFiIKANjr09yMInloHPUKzalaPT7LyyscOKlYyKDtghCsFBHLsod844+dOmthdadHxsvf0QsJpvBjBZDo4NvYcvwMtmXwQk+m1kwGTamv4AczMzcP9BD1y323C/VILrq0tAJ+dPiro5bSEe27st3JbWKZXgrz/+hJ9+HoHr9hW839qkwPSkqaF1DEM7AJkjN7JEH5GSuJHgdfHJOop17OkPPBEa70DQCu9OkvSj65ubnZExCLCxtSXCF3XDz3gGO0aMbYTjB8Xm15lZnrshwPvNTTtERYHL9byauY4jsXOLatQgwDdV/LuMnlu+qwt3BzpigKo+CptSvLKN5wJnPU3XlWidwsUj8pj6Uj84+pQ58gOViZPqpPFVVExNjv3yI86Dez6OOxU9JyXyS0zkgfupw/DXAwx2wn+qu7Ore6JCuwAAAABJRU5ErkJggg==\",\"id\": \"vesp4\"},{\"image\": \"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\",\"id\": \"vesp5\"}]],\"loop\": [{\"type\": \"sprite\",\"x\": 0,\"y\": 36,\"sprite\": 0,\"id\": \"sfb8ry\"}]}";
const char* peppe;

  if (server.startsWith("raw.")) {
    port = 443;
    file = String("/robegamesios/clock-club/main/shared" + file);
  }

  if (ClockwiseParams::getInstance()->canvasFile == "vespa") {
//    DeserializationError error = deserializeJson(doc, vespa);
    peppe = vespa;

  }
  else if (ClockwiseParams::getInstance()->canvasFile == "snoopy") {
//    DeserializationError error = deserializeJson(doc, snoopy3);
    peppe = snoopy3;
  }
  else {
    peppe = vespa;
    if (server == "LOCAL") {
      Serial.print("local canvas name not valid. Load vespa");
    }
  }

  DeserializationError error = deserializeJson(doc, peppe);
  //server = "LOCAL";
  if (server != "LOCAL") {
    Serial.println("NOT LOCAL");
    ClockwiseHttpClient::getInstance()->httpGet(&client, server.c_str(), file.c_str(), port);
    DeserializationError error = deserializeJson(doc, client);
  }
  
//  DeserializationError error = deserializeJson(doc, client);

if (error)
  {
    drawSplashScreen(0xC904, "Error! Check logs");

    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    client.stop();
    return false;
  }

  //TODO check if json is valid

  Serial.printf("[Canvas] Building clockface '%s' by %s, version %d\n", doc["name"].as<const char *>(), doc["author"].as<const char *>(), doc["version"].as<const uint16_t>());
  client.stop();
  return true;
}
