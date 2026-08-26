## ---------------------------------------------------------------------------
## PROPRIETARY CODE – Arthur de Araújo Farias 2025
## All rights reserved.  No part of this file may be reproduced, stored in a
## retrieval system, or transmitted in any form or by any means—electronic,
## mechanical, photocopying, recording, or otherwise—without the prior written
## permission of the copyright holder.
## ---------------------------------------------------------------------------

add_executable(cxflow-example-fake-pipeline cxflow-example-fake-pipeline.cpp)
target_link_libraries(cxflow-example-fake-pipeline PRIVATE cxflow)

add_executable(cxflow-example-custom-elements cxflow-example-custom-elements.cpp)
target_link_libraries(cxflow-example-custom-elements PRIVATE cxflow)
